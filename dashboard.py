import asyncio
import websockets
import queue
import pandas as pd
import json 
import yfinance as yf
import threading
import numpy as np
import seaborn as sns
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
    if not q.empty():
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
       corr_matrix = np.log(data / data.shift(1)).dropna().corr()
       sns.heatmap(corr_matrix, ax=axs[1, 1], annot=True, xticklabels=tickers, yticklabels=tickers, cbar=False)
    plt.draw()

def getMu(data):
    return np.log(data / data.shift(1)).mean() * 252


def packageJSON(data, mu):
    f = open("data.json", "wb")
    tickers = ["AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"]
    stocks_list = [{"name": name, "stockPrice": price, "mu": m}
                   for name, price, m in zip(tickers, data.iloc[-1].tolist(), mu)]
    totalValue = sum(stock["stockPrice"] for stock in stocks_list)
    jsonData = {
        "stocks": stocks_list,
        "covariance": (np.log(data / data.shift(1)).dropna().cov() * 252).values.tolist(),
        "weights": [1/5] * 5,
        "totalValue": totalValue
    }
    output = json.dumps(jsonData)
    f.write(output.encode())
    f.close()


# --- Main Code ---
print("Starting Monte Carlo client...")
df = pd.DataFrame()
tickers = ["AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"]
data = yf.download(tickers, period="2y")["Close"]
mu = getMu(data)
packageJSON(data, mu)
q = queue.Queue()
interval = 0.25
threading.Thread(target=asyncio.run, args=(websocket(),), daemon=True).start()
fig, axs = plt.subplots(2, 2)
ani = FuncAnimation(fig, updatePlot, interval=50)
plt.show()  