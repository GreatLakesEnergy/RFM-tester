import argparse
import os
import serial

print os.listdir("/Users/tom/Documents/Python/")

"""
for filename in os.listdir("/Users/tom/Documents/Python/"):
	info = os.stat(filename)
	print info
"""

info = os.stat("Aphex.mp3")
print info




# End