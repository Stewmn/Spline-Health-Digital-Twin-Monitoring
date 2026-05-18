import asyncio
import serial
import websockets

# Configuration
SERIAL_PORT = "COM8"
BAUD_RATE = 115200
WS_PORT = 8765

clients = set()

async def serial_reader():
    try:
        # Added a short timeout to prevent the loop from hanging
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
        print(f"[OK] Serial connected: {SERIAL_PORT}")
    except Exception as e:
        print(f"[Error] Could not open serial port: {e}")
        return

    while True:
        try:
            if ser.in_waiting > 0:
                raw_data = ser.readline()
                # The 'ignore' flag fixes the 'utf-8' codec error
                line = raw_data.decode("utf-8", errors="ignore").strip()
                
                if line.startswith("BEND:") and clients:
                    print(f"Broadcasting: {line}") # Debug print
                    await asyncio.gather(*[c.send(line) for c in clients])
        except Exception as e:
            print(f"[Serial error] {e}")
        
        await asyncio.sleep(0.01) # Small delay to be CPU friendly

async def ws_handler(ws):
    clients.add(ws)
    print(f"[+] Browser connected. Total: {len(clients)}")
    try:
        await ws.wait_closed()
    finally:
        clients.discard(ws)
        print(f"[-] Browser disconnected. Total: {len(clients)}")

async def main():
    print(f"[WS] Listening on ws://localhost:{WS_PORT}")
    # Start the websocket server
    async with websockets.serve(ws_handler, "0.0.0.0", WS_PORT):
        # Run the serial reader
        await serial_reader()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[!] Bridge stopped by user.")
