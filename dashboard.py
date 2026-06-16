import asyncio
import websockets
import queue
import pandas as pd
import json 
import yfinance as yf
import threading
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import time

## main() returns a coroutine object.
async def websocket():
    async with websockets.connect("ws://localhost:9001/ws") as websocket:
        while True:
            message = await websocket.recv()
            data = json.loads(message)
            q.put(data)

def updatePlot(frame):
    global df
    df = pd.concat([df, pd.DataFrame([q.get()])], ignore_index=True)
    for ax in axs.flat:
        ax.clear()
    if not df.empty:
       axs[0, 0].plot(df["var95"])
       axs[0, 0].set_title("Var95")
       axs[0, 1].plot(df["cvar95"])
       axs[0, 1].set_title("CVar95")
       axs[1, 0].plot(df["mean"])
       axs[1, 0].set_title("Mean")
    plt.draw()

# --- Main Code ---
print("Starting Monte Carlo client...")
data = yf.download(["AAPL", "MSFT", "GOOGL", "AMZN" , "TSLA"], period = "2y")["Close"]
print(data)
q = queue.Queue()
df = pd.DataFrame()
interval = 0.25
threading.Thread(target=asyncio.run, args=(websocket(),), daemon=True).start()
fig, axs = plt.subplots(2, 2)
ani = FuncAnimation(fig, updatePlot, interval=50)
plt.show()  