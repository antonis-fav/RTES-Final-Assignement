import csv
import os
from matplotlib import pyplot as plt
import numpy as np


def Average(a):
  #average function
  avg = np.average(a)
  return(avg)


data_points = 10

i = 0
j = 0 
k = []
input = ["./24_wres_8_threads/BINANCE:BTCUSDT/BINANCE:BTCUSDT_timestraps.txt",
         "./24_wres_8_threads/AMZN/AMZN_timestraps.txt",
         "./24_wres_8_threads/APPL/APPL_timestraps.txt",
         "./24_wres_8_threads/IC MARKETS:1/IC MARKETS:1_timestraps.txt"]

found_stocks=[]

for j in range (0, 4, 1):
    
    try:
        
        with open(input[j], 'r') as file:
            if(j == 0):
                found_stocks.append('BTCUSDT')
            elif(j == 1):
                found_stocks.append('AMZN')
            elif(j == 2):
                found_stocks.append('APPL')
            elif(j == 3):
                found_stocks.append('IC MARKETS:1')
            
            a=[]
            i=0
            for line in file:
                chunks = line.split(',')
                a.append((int(chunks[1])-int(chunks[0]))/1000)
                i = i + 1

            k.append(sum(a)/i)
    except FileNotFoundError as e:
        print(e)
x_axis = ['A', 'B', 'C']
# plt.grid()
plt.plot(x_axis,k, marker = 's')
plt.title('24 Hours-Data - 8 Threads')
plt.ylabel('Median Delay (seconds)')
plt.xlabel('Different Stocks')
plt.legend()
plt.show()





input = ["./2_wres_1_threads/BINANCE:BTCUSDT/BINANCE:BTCUSDT_timestraps.txt",
         "./2_wres_2_threads/BINANCE:BTCUSDT/BINANCE:BTCUSDT_timestraps.txt",
         "./2_wres_8_threads/BINANCE:BTCUSDT/BINANCE:BTCUSDT_timestraps.txt",
         "./2_wres_16_threads/BINANCE:BTCUSDT/BINANCE:BTCUSDT_timestraps.txt"]



n_threads = []
for j in range (0, 4, 1):
    
    try:
        
        with open(input[j], 'r') as file:
            if(j == 0):
                n_threads = '1 Thread'
            elif(j== 1):
                n_threads = '2 Threads'
            elif(j== 2):
                n_threads = '8 Threads'
            elif(j== 3):
                n_threads = '16 Threads'

            a=[]
            for line in file:
                chunks = line.split(',')
                a.append((int(chunks[1])-int(chunks[0]))/1000)
                i = i + 1


            list_avg = np.array_split(a, data_points)
            print(len(list_avg))
            k = []
            for i in range(0, data_points, 1):
                k.append(Average(list_avg[i]))
            plt.plot(k, label= n_threads)

    except FileNotFoundError as e:
        print(e)

plt.grid()
plt.title('2 Hours-Data')
plt.ylabel('Mean Delay (seconds)')
plt.xlabel('Hours Data / Data points ')
plt.legend()
plt.show()


input = ["./2_wres_1_threads/BINANCE:BTCUSDT/BINANCE:BTCUSDT_timestraps.txt",
         "./2_wres_2_threads/BINANCE:BTCUSDT/BINANCE:BTCUSDT_timestraps.txt",
         "./2_wres_8_threads/BINANCE:BTCUSDT/BINANCE:BTCUSDT_timestraps.txt",
         "./2_wres_16_threads/BINANCE:BTCUSDT/BINANCE:BTCUSDT_timestraps.txt"]


n_threads = []
mean_all = []
for j in range (0, 4, 1):
    
    try:
        
        with open(input[j], 'r') as file:
            if(j == 0):
                n_threads = '1 Thread'
            elif(j== 1):
                n_threads = '2 Threads'
            elif(j== 2):
                n_threads = '8 Threads'
            elif(j== 3):
                n_threads = '16 Threads'

            a=[]
            i=0
            for line in file:
                chunks = line.split(',')
                a.append((int(chunks[1])-int(chunks[0]))/1000)
                i = i + 1


            mean_all.append(sum(a)/i)

    except FileNotFoundError as e:
        print(e)

x_axis = [1, 2, 8, 16]
plt.plot(x_axis, mean_all, marker = 'd')
plt.grid()
plt.title('2 Hours-Data')
plt.ylabel('Mean Delay (seconds)')

plt.xlabel('Number of threads')
# plt.legend()
plt.show()

input = ["./8_wres_8_threads/BINANCE:BTCUSDT/BINANCE:BTCUSDT_timestraps.txt",
         "./8_wres_8_threads/AMZN/AMZN_timestraps.txt",
         "./8_wres_8_threads/APPL/APPL_timestraps.txt",
         "./8_wres_8_threads/IC MARKETS:1/IC MARKETS:1_timestraps.txt"]
found_stocks = []
k = []
for j in range (0, 4, 1):
    
    try:
        
        with open(input[j], 'r') as file:
            if(j == 0):
                found_stocks.append('BTCUSDT')
            elif(j == 1):
                found_stocks.append('AMZN')
            elif(j == 2):
                found_stocks.append('APPL')
            elif(j == 3):
                found_stocks.append('IC MARKETS:1')

            a=[]
            i=0
            for line in file:
                chunks = line.split(',')
                a.append((int(chunks[1])-int(chunks[0]))/1000)
                i = i + 1

            k.append(sum(a)/i)

    except FileNotFoundError as e:
        print(e)

print(k)

plt.plot(k, marker = 's', label = '8 Threads')

plt.title('8 Hours-Data')
plt.ylabel('Median Delay (seconds)')
plt.xlabel('Stocks')
plt.legend()



input = ["./8_wres_2_threads/BINANCE:BTCUSDT/BINANCE:BTCUSDT_timestraps.txt",
         "./8_wres_2_threads/AMZN/AMZN_timestraps.txt",
         "./8_wres_2_threads/APPL/APPL_timestraps.txt",
         "./8_wres_2_threads/IC MARKETS:1/IC MARKETS:1_timestraps.txt"]
found_stocks = []
k = []
for j in range (0, 4, 1):
    
    try:
        
        with open(input[j], 'r') as file:
            if(j == 0):
                found_stocks.append('BTCUSDT')
            elif(j == 1):
                found_stocks.append('AMZN')
            elif(j == 2):
                found_stocks.append('APPL')
            elif(j == 3):
                found_stocks.append('IC MARKETS:1')

            a=[]
            i=0
            for line in file:
                chunks = line.split(',')
                a.append((int(chunks[1])-int(chunks[0]))/1000)
                i = i + 1

            k.append(sum(a)/i)

    except FileNotFoundError as e:
        print(e)

print(found_stocks)

x_axis=['A', 'B', 'C']
plt.plot(x_axis, k, marker ='o', label = '2 Threads')

plt.title('8 Hours-Data')
plt.ylabel('Median Delay (seconds)')
plt.xlabel('Different Stocks')
plt.legend()
plt.show()
