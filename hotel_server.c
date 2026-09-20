#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <direct.h>
typedef SOCKET socket_t;
#define CLOSESOCK closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int socket_t;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define CLOSESOCK close
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define PORT 8090
#define MAX_ROOMS 100
#define MAX_CUSTOMERS 2000
#define MAX_SERVICES 6000
#define MAX_BILLS 2000
#define BUF 65536
#define DATA_DIR "data"
#define LOGIN_EMAIL "admin@gmail.com"
#define LOGIN_PASSWORD "admin123"
#define GST_RATE 0.05

typedef struct { int room_no; char type[20]; double price_inr; } Room;
typedef struct { int id; char name[80]; char contact[30]; int room_no; char check_in[11]; char check_out[11]; char status[16]; } Customer;
typedef struct { int customer_id; char name[60]; double amount_inr; int days; } Service;
typedef struct { int id; int customer_id; int room_no; char check_in[11]; char check_out[11]; int nights; double room_charge_inr; double service_charge_inr; double subtotal_inr; double discount_inr; double gst_inr; double total_inr; char reward[120]; char currency[8]; char created_at[20]; } Bill;
typedef struct { char code[8]; char symbol[12]; double rate_from_inr; } Currency;

static Room rooms[MAX_ROOMS]; static int room_count;
static Customer customers[MAX_CUSTOMERS]; static int customer_count;
static Service services[MAX_SERVICES]; static int service_count;
static Bill bills[MAX_BILLS]; static int bill_count;
static int logged_in=0;
static Currency selected_currency={"INR","₹",1.0};
static Currency currencies[] = {
 {"INR","₹",1.0},{"USD","$",0.0117},{"GBP","£",0.0086},{"AED","د.إ",0.043},
 {"CAD","C$",0.0159},{"AUD","A$",0.0180},{"SGD","S$",0.0150},{"JPY","¥",1.72}
};
static const int currency_count=8;

typedef struct { char name[20]; double rate_inr_per_day; } ServiceRate;
static ServiceRate service_rates[] = {
 {"WiFi",50},{"Laundry",100},{"Cleaning",50},{"Food",200}
};
static const int service_rate_count=4;

static void ensure_data_dir(void){
#ifdef _WIN32
 _mkdir(DATA_DIR);
#else
 char cmd[64]; snprintf(cmd,sizeof(cmd),"mkdir -p %s",DATA_DIR); system(cmd);
#endif
}
static void seed_rooms(void){
 Room seed[]={{101,"Single",1000},{102,"Single",1000},{201,"Double",1500},{202,"Double",1500},{301,"Deluxe",2300},{302,"Deluxe",2200},{401,"Suite",3600}};
 room_count=(int)(sizeof(seed)/sizeof(seed[0])); memcpy(rooms,seed,sizeof(seed));
}
static void load_data(void){
 ensure_data_dir(); seed_rooms();
 FILE *f=fopen(DATA_DIR "/customers.dat","r");
 if(f){while(customer_count<MAX_CUSTOMERS && fscanf(f,"%d|%79[^|]|%29[^|]|%d|%10[^|]|%10[^|]|%15[^\n]\n",&customers[customer_count].id,customers[customer_count].name,customers[customer_count].contact,&customers[customer_count].room_no,customers[customer_count].check_in,customers[customer_count].check_out,customers[customer_count].status)==7)customer_count++;fclose(f);}
 f=fopen(DATA_DIR "/services.dat","r");
 if(f){while(service_count<MAX_SERVICES && fscanf(f,"%d|%59[^|]|%lf|%d\n",&services[service_count].customer_id,services[service_count].name,&services[service_count].amount_inr,&services[service_count].days)==4)service_count++;fclose(f);}
 f=fopen(DATA_DIR "/bills.dat","r");
 if(f){while(bill_count<MAX_BILLS && fscanf(f,"%d|%d|%d|%10[^|]|%10[^|]|%d|%lf|%lf|%lf|%lf|%lf|%lf|%119[^|]|%7[^|]|%19[^\n]\n",&bills[bill_count].id,&bills[bill_count].customer_id,&bills[bill_count].room_no,bills[bill_count].check_in,bills[bill_count].check_out,&bills[bill_count].nights,&bills[bill_count].room_charge_inr,&bills[bill_count].service_charge_inr,&bills[bill_count].subtotal_inr,&bills[bill_count].discount_inr,&bills[bill_count].gst_inr,&bills[bill_count].total_inr,bills[bill_count].reward,bills[bill_count].currency,bills[bill_count].created_at)==15)bill_count++;fclose(f);}
}
static void save_customers(void){FILE*f=fopen(DATA_DIR"/customers.dat","w");if(!f)return;for(int i=0;i<customer_count;i++)fprintf(f,"%d|%s|%s|%d|%s|%s|%s\n",customers[i].id,customers[i].name,customers[i].contact,customers[i].room_no,customers[i].check_in,customers[i].check_out,customers[i].status);fclose(f);}
static void save_services(void){FILE*f=fopen(DATA_DIR"/services.dat","w");if(!f)return;for(int i=0;i<service_count;i++)fprintf(f,"%d|%s|%.2f|%d\n",services[i].customer_id,services[i].name,services[i].amount_inr,services[i].days);fclose(f);}
static void save_bills(void){FILE*f=fopen(DATA_DIR"/bills.dat","w");if(!f)return;for(int i=0;i<bill_count;i++)fprintf(f,"%d|%d|%d|%s|%s|%d|%.2f|%.2f|%.2f|%.2f|%.2f|%.2f|%s|%s|%s\n",bills[i].id,bills[i].customer_id,bills[i].room_no,bills[i].check_in,bills[i].check_out,bills[i].nights,bills[i].room_charge_inr,bills[i].service_charge_inr,bills[i].subtotal_inr,bills[i].discount_inr,bills[i].gst_inr,bills[i].total_inr,bills[i].reward,bills[i].currency,bills[i].created_at);fclose(f);}

static int date_value(const char*s){int y,m,d;if(!s||sscanf(s,"%d-%d-%d",&y,&m,&d)!=3)return -1;if(y<1900||m<1||m>12||d<1||d>31)return -1;return y*10000+m*100+d;}
static int days_from_civil(int y,unsigned m,unsigned d){y-=m<=2;const int era=(y>=0?y:y-399)/400;const unsigned yoe=(unsigned)(y-era*400);const unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;const unsigned doe=yoe*365+yoe/4-yoe/100+doy;return era*146097+(int)doe-719468;}
static int nights_between(const char*a,const char*b){int y1,m1,d1,y2,m2,d2;if(sscanf(a,"%d-%d-%d",&y1,&m1,&d1)!=3||sscanf(b,"%d-%d-%d",&y2,&m2,&d2)!=3)return 0;int n=days_from_civil(y2,m2,d2)-days_from_civil(y1,m1,d1);return n>0?n:0;}
static int overlaps(const char*ni,const char*no,const char*oi,const char*oo){int a=date_value(ni),b=date_value(no),c=date_value(oi),d=date_value(oo);return a>=0&&b>a&&c>=0&&d>c&&!(b<=c||a>=d);}
static int room_index(int rn){for(int i=0;i<room_count;i++)if(rooms[i].room_no==rn)return i;return -1;}
static int customer_index(int id){for(int i=0;i<customer_count;i++)if(customers[i].id==id)return i;return -1;}
static int next_customer_id(void){int m=1000;for(int i=0;i<customer_count;i++)if(customers[i].id>m)m=customers[i].id;return m+1;}
static double convert(double inr){return inr*selected_currency.rate_from_inr;}
static int same_guest_overlap(const char*n,const char*p,const char*in,const char*out){for(int i=0;i<customer_count;i++)if(!strcmp(customers[i].status,"Booked")&&!strcmp(customers[i].name,n)&&!strcmp(customers[i].contact,p)&&overlaps(in,out,customers[i].check_in,customers[i].check_out))return 1;return 0;}
static int room_available(int rn,const char*in,const char*out){for(int i=0;i<customer_count;i++)if(!strcmp(customers[i].status,"Booked")&&customers[i].room_no==rn&&overlaps(in,out,customers[i].check_in,customers[i].check_out))return 0;return 1;}
static int guest_booking_count(const char*n,const char*p){int count=0;for(int i=0;i<customer_count;i++)if(!strcmp(customers[i].name,n)&&!strcmp(customers[i].contact,p)&&(strcmp(customers[i].status,"Cancelled")!=0))count++;return count;}
static double service_rate(const char*n){for(int i=0;i<service_rate_count;i++)if(!strcmp(service_rates[i].name,n))return service_rates[i].rate_inr_per_day;return -1;}

static const char*param(const char*body,const char*key,char*out,size_t n){if(!body)return NULL;size_t k=strlen(key);const char*p=body;while(*p){if((p==body||p[-1]=='&')&&!strncmp(p,key,k)&&p[k]=='='){p+=k+1;size_t i=0;while(p[i]&&p[i]!='&'&&i<n-1){out[i]=p[i];i++;}out[i]=0;return out;}while(*p&&*p!='&')p++;if(*p=='&')p++;}return NULL;}
static void urldecode(char*s){char*o=s;for(char*p=s;*p;p++){if(*p=='+')*o++=' ';else if(*p=='%'&&isxdigit((unsigned char)p[1])&&isxdigit((unsigned char)p[2])){char h[3]={p[1],p[2],0};*o++=(char)strtol(h,NULL,16);p+=2;}else *o++=*p;}*o=0;}
static int body_param(const char*b,const char*k,char*o,size_t n){if(!param(b,k,o,n))return 0;urldecode(o);return 1;}
static void response(socket_t s,int code,const char*type,const char*extra,const char*body){const char*msg=code==200?"OK":code==302?"Found":code==400?"Bad Request":code==401?"Unauthorized":code==404?"Not Found":code==409?"Conflict":"Internal Server Error";char h[1200];snprintf(h,sizeof(h),"HTTP/1.1 %d %s\r\nContent-Type: %s; charset=utf-8\r\nContent-Length: %d\r\nConnection: close\r\n%s\r\n",code,msg,type,(int)strlen(body),extra?extra:"");send(s,h,(int)strlen(h),0);send(s,body,(int)strlen(body),0);}
static void json(socket_t s,int code,const char*body){response(s,code,"application/json",NULL,body);}
static void file_response(socket_t s,const char*path,const char*type){FILE*f=fopen(path,"rb");if(!f){response(s,404,"text/plain",NULL,"Not found");return;}fseek(f,0,SEEK_END);long z=ftell(f);fseek(f,0,SEEK_SET);char*b=(char*)malloc((size_t)z+1);if(!b){fclose(f);response(s,500,"text/plain",NULL,"Memory error");return;}fread(b,1,(size_t)z,f);b[z]=0;fclose(f);response(s,200,type,NULL,b);free(b);}

static void api_login(socket_t s,char*b){char e[100]={0},p[100]={0},c[20]={0};body_param(b,"email",e,sizeof(e));body_param(b,"password",p,sizeof(p));body_param(b,"country",c,sizeof(c));if(strcmp(e,LOGIN_EMAIL)||strcmp(p,LOGIN_PASSWORD)){json(s,401,"{\"ok\":false,\"message\":\"Invalid email or password\"}");return;}logged_in=1;for(int i=0;i<currency_count;i++)if(!strcmp(c,currencies[i].code)){selected_currency=currencies[i];break;}json(s,200,"{\"ok\":true,\"message\":\"Login successful\"}");}
static int auth(socket_t s){if(!logged_in){json(s,401,"{\"ok\":false,\"message\":\"Please login first\"}");return 0;}return 1;}

static void api_rooms(socket_t s,const char*q){char typ[30]={0},in[20]={0},out[20]={0};if(q){body_param(q,"type",typ,sizeof(typ));body_param(q,"check_in",in,sizeof(in));body_param(q,"check_out",out,sizeof(out));}char*b=(char*)malloc(20000);size_t p=0;p+=sprintf(b+p,"{\"ok\":true,\"currency\":\"%s\",\"symbol\":\"%s\",\"rooms\":[",selected_currency.code,selected_currency.symbol);int first=1;for(int i=0;i<room_count;i++){if(typ[0]&&strcmp(rooms[i].type,typ))continue;int av=(in[0]&&out[0])?room_available(rooms[i].room_no,in,out):room_available(rooms[i].room_no,"2099-01-01","2099-01-02");if(!first)p+=sprintf(b+p,",");first=0;p+=sprintf(b+p,"{\"room_no\":%d,\"type\":\"%s\",\"price\":%.2f,\"available\":%s}",rooms[i].room_no,rooms[i].type,convert(rooms[i].price_inr),av?"true":"false");}p+=sprintf(b+p,"]}");json(s,200,b);free(b);}
static void api_stats(socket_t s){int av=0,act=0;for(int i=0;i<room_count;i++)if(room_available(rooms[i].room_no,"2099-01-01","2099-01-02"))av++;for(int i=0;i<customer_count;i++)if(!strcmp(customers[i].status,"Booked"))act++;char b[1000];snprintf(b,sizeof(b),"{\"ok\":true,\"total_rooms\":%d,\"available_rooms\":%d,\"active_bookings\":%d,\"currency\":\"%s\",\"symbol\":\"%s\"}",room_count,av,act,selected_currency.code,selected_currency.symbol);json(s,200,b);}
static void api_rates(socket_t s){char b[1500];size_t p=0;p+=sprintf(b+p,"{\"ok\":true,\"services\":[");for(int i=0;i<service_rate_count;i++){if(i)p+=sprintf(b+p,",");p+=sprintf(b+p,"{\"name\":\"%s\",\"rate\":%.2f}",service_rates[i].name,convert(service_rates[i].rate_inr_per_day));}p+=sprintf(b+p,"]}");json(s,200,b);}

static void api_book(socket_t s,char*b){char n[100]={0},p[40]={0},r[20]={0},in[20]={0},out[20]={0};body_param(b,"name",n,sizeof(n));body_param(b,"contact",p,sizeof(p));body_param(b,"room_number",r,sizeof(r));body_param(b,"check_in",in,sizeof(in));body_param(b,"check_out",out,sizeof(out));int rn=atoi(r),ri=room_index(rn);if(!n[0]||!p[0]||ri<0||date_value(in)<0||date_value(out)<=date_value(in)){json(s,400,"{\"ok\":false,\"message\":\"Please enter valid customer, room and dates.\"}");return;}if(same_guest_overlap(n,p,in,out)){json(s,409,"{\"ok\":false,\"message\":\"Double booking prevented: this customer already has an overlapping booking with the same name and contact number.\"}");return;}if(!room_available(rn,in,out)){json(s,409,"{\"ok\":false,\"message\":\"Double booking prevented: this room is already reserved for those dates.\"}");return;}if(customer_count>=MAX_CUSTOMERS){json(s,500,"{\"ok\":false,\"message\":\"Customer storage full.\"}");return;}Customer*c=&customers[customer_count++];c->id=next_customer_id();strncpy(c->name,n,79);strncpy(c->contact,p,29);c->room_no=rn;strncpy(c->check_in,in,10);strncpy(c->check_out,out,10);strcpy(c->status,"Booked");save_customers();int bc=guest_booking_count(n,p);char reward[200];if(bc>=10)snprintf(reward,sizeof(reward),"10+ bookings: Free Food reward eligible at checkout");else if(bc>=3)snprintf(reward,sizeof(reward),"3+ bookings: 10%% loyalty discount eligible at checkout");else snprintf(reward,sizeof(reward),"%d booking(s) completed/active",bc);char outj[800];snprintf(outj,sizeof(outj),"{\"ok\":true,\"message\":\"Room booked successfully.\",\"customer_id\":%d,\"booking_count\":%d,\"reward\":\"%s\"}",c->id,bc,reward);json(s,200,outj);}
static void api_cancel(socket_t s,char*b){char x[30]={0};body_param(b,"customer_id",x,sizeof(x));int id=atoi(x),i=customer_index(id);if(i<0||strcmp(customers[i].status,"Booked")){json(s,404,"{\"ok\":false,\"message\":\"Active booking not found.\"}");return;}strcpy(customers[i].status,"Cancelled");save_customers();json(s,200,"{\"ok\":true,\"message\":\"Booking cancelled.\"}");}
static void api_service(socket_t s,char*b){char x[30]={0},name[50]={0};body_param(b,"customer_id",x,sizeof(x));body_param(b,"service_name",name,sizeof(name));int id=atoi(x),ci=customer_index(id);if(ci<0||strcmp(customers[ci].status,"Booked")){json(s,400,"{\"ok\":false,\"message\":\"Active booking not found.\"}");return;}double rate=service_rate(name);if(rate<0){json(s,400,"{\"ok\":false,\"message\":\"Unknown service.\"}");return;}int n=nights_between(customers[ci].check_in,customers[ci].check_out);if(n<1)n=1;if(service_count>=MAX_SERVICES){json(s,500,"{\"ok\":false,\"message\":\"Service storage full.\"}");return;}services[service_count].customer_id=id;strncpy(services[service_count].name,name,59);services[service_count].days=n;services[service_count].amount_inr=rate*n;service_count++;save_services();char out[600];snprintf(out,sizeof(out),"{\"ok\":true,\"message\":\"%s added for %d day(s).\",\"service\":\"%s\",\"days\":%d,\"amount\":%.2f}",name,n,name,n,convert(rate*n));json(s,200,out);}
static void api_checkout(socket_t s,char*b){char x[30]={0};body_param(b,"customer_id",x,sizeof(x));int id=atoi(x),ci=customer_index(id);if(ci<0||strcmp(customers[ci].status,"Booked")){json(s,404,"{\"ok\":false,\"message\":\"Active booking not found.\"}");return;}Customer*c=&customers[ci];int ri=room_index(c->room_no);if(ri<0){json(s,500,"{\"ok\":false,\"message\":\"Room not found.\"}");return;}int n=nights_between(c->check_in,c->check_out);if(n<1)n=1;double room_charge=rooms[ri].price_inr*n,service=0;for(int i=0;i<service_count;i++)if(services[i].customer_id==id)service+=services[i].amount_inr;int count=guest_booking_count(c->name,c->contact);double discount=0;char reward[200]="No reward";if(count>=10){double free_food=service_rate("Food")*n;if(service>=free_food)service-=free_food;else if(service>0)service=0;snprintf(reward,sizeof(reward),"FREE FOOD reward applied (10+ bookings)");}else if(count>=3){double sub=room_charge+service;discount=sub*0.10;snprintf(reward,sizeof(reward),"10%% loyalty discount applied (3+ bookings)");}double subtotal=room_charge+service;double after_discount=subtotal-discount;double gst=after_discount*GST_RATE;double total=after_discount+gst;if(bill_count>=MAX_BILLS){json(s,500,"{\"ok\":false,\"message\":\"Bill storage full.\"}");return;}Bill*bill=&bills[bill_count];bill->id=bill_count+1;bill->customer_id=id;bill->room_no=c->room_no;strncpy(bill->check_in,c->check_in,10);bill->check_in[10]=0;strncpy(bill->check_out,c->check_out,10);bill->check_out[10]=0;bill->nights=n;bill->room_charge_inr=room_charge;bill->service_charge_inr=service;bill->subtotal_inr=subtotal;bill->discount_inr=discount;bill->gst_inr=gst;bill->total_inr=total;strncpy(bill->reward,reward,119);strncpy(bill->currency,selected_currency.code,7);time_t now=time(NULL);struct tm*tmv=localtime(&now);strftime(bill->created_at,20,"%Y-%m-%d %H:%M:%S",tmv);bill_count++;strcpy(c->status,"Checked Out");save_customers();save_bills();char out[1800];snprintf(out,sizeof(out),"{\"ok\":true,\"message\":\"Checkout completed and final bill generated.\",\"bill\":{\"id\":%d,\"customer_id\":%d,\"name\":\"%s\",\"contact\":\"%s\",\"room_no\":%d,\"nights\":%d,\"room_charge\":%.2f,\"service_charge\":%.2f,\"subtotal\":%.2f,\"discount\":%.2f,\"gst\":%.2f,\"gst_rate\":%.0f,\"total\":%.2f,\"reward\":\"%s\",\"currency\":\"%s\",\"symbol\":\"%s\"}}",bill->id,id,c->name,c->contact,c->room_no,n,convert(room_charge),convert(service),convert(subtotal),convert(discount),convert(gst),GST_RATE*100,convert(total),reward,selected_currency.code,selected_currency.symbol);json(s,200,out);}

static void api_bookings(socket_t s){char*b=(char*)malloc(50000);size_t p=0;p+=sprintf(b+p,"{\"ok\":true,\"bookings\":[");int first=1;for(int i=0;i<customer_count;i++)if(!strcmp(customers[i].status,"Booked")){if(!first)p+=sprintf(b+p,",");first=0;int count=guest_booking_count(customers[i].name,customers[i].contact);char reward[100];if(count>=10)strcpy(reward,"Free food");else if(count>=3)strcpy(reward,"10% discount");else strcpy(reward,"No reward yet");p+=sprintf(b+p,"{\"customer_id\":%d,\"name\":\"%s\",\"contact\":\"%s\",\"room_no\":%d,\"check_in\":\"%s\",\"check_out\":\"%s\",\"booking_count\":%d,\"reward\":\"%s\"}",customers[i].id,customers[i].name,customers[i].contact,customers[i].room_no,customers[i].check_in,customers[i].check_out,count,reward);}p+=sprintf(b+p,"]}");json(s,200,b);free(b);}

static void api_revenue(socket_t s){
 char*b=(char*)malloc(60000);size_t p=0;p+=sprintf(b+p,"{\"ok\":true,\"currency\":\"%s\",\"symbol\":\"%s\",\"guests\":[",selected_currency.code,selected_currency.symbol);int first=1;
 for(int ci=0;ci<customer_count;ci++){
  if(customers[ci].status[0]=='\0')continue;
  int exists=0;for(int k=0;k<ci;k++)if(!strcmp(customers[k].name,customers[ci].name)&&!strcmp(customers[k].contact,customers[ci].contact))exists=1;if(exists)continue;
  double total=0;int bookings=0;double last=0;char reward[120]="No reward yet";
  for(int i=0;i<bill_count;i++){int id=bills[i].customer_id;int x=customer_index(id);if(x>=0&&!strcmp(customers[x].name,customers[ci].name)&&!strcmp(customers[x].contact,customers[ci].contact)){total+=bills[i].total_inr;bookings++;last=bills[i].total_inr;}}
  int bc=guest_booking_count(customers[ci].name,customers[ci].contact);if(bc>=10)strcpy(reward,"Free food reward");else if(bc>=3)strcpy(reward,"10% discount reward");
  if(!first)p+=sprintf(b+p,",");first=0;p+=sprintf(b+p,"{\"name\":\"%s\",\"contact\":\"%s\",\"booking_count\":%d,\"completed_bills\":%d,\"total_spend\":%.2f,\"last_bill\":%.2f,\"reward\":\"%s\"}",customers[ci].name,customers[ci].contact,bc,bookings,convert(total),convert(last),reward);
 }
 p+=sprintf(b+p,"]}");json(s,200,b);free(b);
}

static void handle(socket_t s,char*req,int len){req[len]=0;char method[8]={0},target[512]={0};sscanf(req,"%7s %511s",method,target);char*body=strstr(req,"\r\n\r\n");if(body)body+=4;else body="";
 if(!strcmp(target,"/")||!strcmp(target,"/index.html")){file_response(s,"public/index.html","text/html");return;}if(!strcmp(target,"/app.js")){file_response(s,"public/app.js","application/javascript");return;}if(!strcmp(target,"/style.css")){file_response(s,"public/style.css","text/css");return;}
 if(strncmp(target,"/images/",8)==0){char path[600];snprintf(path,sizeof(path),"public%s",target);file_response(s,path,"image/svg+xml");return;}
 char path[512];strncpy(path,target,sizeof(path)-1);char*q=strchr(path,'?');if(q)*q=0;
 if(!strcmp(path,"/api/login")&&!strcmp(method,"POST")){api_login(s,body);return;}if(!strcmp(path,"/api/logout")){logged_in=0;json(s,200,"{\"ok\":true}");return;}if(!auth(s))return;
 if(!strcmp(path,"/api/rooms")){api_rooms(s,q?q+1:NULL);return;}if(!strcmp(path,"/api/stats")){api_stats(s);return;}if(!strcmp(path,"/api/rates")){api_rates(s);return;}if(!strcmp(path,"/api/bookings")){api_bookings(s);return;}if(!strcmp(path,"/api/revenue")){api_revenue(s);return;}
 if(!strcmp(path,"/api/book")&&!strcmp(method,"POST")){api_book(s,body);return;}if(!strcmp(path,"/api/cancel")&&!strcmp(method,"POST")){api_cancel(s,body);return;}if(!strcmp(path,"/api/service")&&!strcmp(method,"POST")){api_service(s,body);return;}if(!strcmp(path,"/api/checkout")&&!strcmp(method,"POST")){api_checkout(s,body);return;}response(s,404,"text/plain",NULL,"Route not found");
}

int main(void){
#ifdef _WIN32
 WSADATA w; if(WSAStartup(MAKEWORD(2,2),&w)!=0){printf("WSAStartup failed\n");return 1;}
#endif
 load_data();socket_t server=socket(AF_INET,SOCK_STREAM,0);if(server==INVALID_SOCKET){printf("Socket creation failed\n");return 1;}int opt=1;setsockopt(server,SOL_SOCKET,SO_REUSEADDR,(const char*)&opt,sizeof(opt));struct sockaddr_in a;memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_ANY);a.sin_port=htons(PORT);if(bind(server,(struct sockaddr*)&a,sizeof(a))==SOCKET_ERROR){printf("Bind failed. Port %d may be busy.\n",PORT);return 1;}if(listen(server,10)==SOCKET_ERROR){printf("Listen failed\n");return 1;}
 printf("\n==============================================\n GrandStay Nature Resort - C Web Backend\n Open: http://127.0.0.1:%d\n Login: %s / %s\n==============================================\n\n",PORT,LOGIN_EMAIL,LOGIN_PASSWORD);
 while(1){struct sockaddr_in client;
#ifdef _WIN32
 int clen=sizeof(client);
#else
 socklen_t clen=sizeof(client);
#endif
 socket_t s=accept(server,(struct sockaddr*)&client,&clen);if(s==INVALID_SOCKET)continue;char buf[BUF];int n=(int)recv(s,buf,sizeof(buf)-1,0);if(n>0)handle(s,buf,n);CLOSESOCK(s);}
 return 0;
}
