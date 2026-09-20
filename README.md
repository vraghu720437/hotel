# GrandStay Nature Resort — Hotel Room Booking Management System (C Web)

## Technology
- C language backend using Winsock on Windows
- HTML/CSS/JavaScript frontend
- File-based persistent storage (`data/*.dat`)
- Browser interface: `http://127.0.0.1:8090`

## Login
Email: `admin@gmail.com`
Password: `admin123`

The login is a Google-style design only; it is not Google OAuth.

## Room prices (INR base)
- Single: ₹900/day
- Double: ₹1,400/day
- Deluxe: ₹2,200/day
- Suite: ₹3,500/day

## Fixed daily services
- WiFi: ₹50/day
- Laundry: ₹100/day
- Cleaning: ₹50/day
- Food: ₹200/day

When a service is added, the program automatically multiplies the fixed daily rate by the guest's stay length. Staff do not type a service amount.

## Billing
Final bill contains:
- Room charges
- Additional service charges
- Subtotal
- Loyalty discount/reward
- GST (5%)
- Grand total
- Selected country currency

The backend stores all monetary values in INR and converts them to the selected display currency.

## Loyalty tasks
- 3 or more bookings by the same guest (same name + contact): 10% loyalty discount at checkout.
- 10 or more bookings: free-food reward. The standard food service charge for the stay is removed at checkout.
- Guest Revenue & Rewards shows revenue per guest rather than a generic hotel revenue total.

## Double-booking prevention
The backend rejects:
1. The same room when reservation dates overlap.
2. The same customer name + contact number when reservation dates overlap.

Customer ID is generated automatically by the hotel.

## Run on Windows with MinGW
Open PowerShell in the folder containing `hotel_server.c` and run:

```powershell
gcc hotel_server.c -o hotel_server.exe -lws2_32
.\hotel_server.exe
```

Then open Chrome:

`http://127.0.0.1:8090`

Keep the server terminal open while using the website.

## Note
This is an academic/demo C web server. A production system should use secure authentication, HTTPS, a real database, stronger input validation, and multi-user session management.
