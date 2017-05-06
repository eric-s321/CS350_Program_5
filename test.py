

fileOut = open("testInput.txt", "w")
for i in range(70000):
	fileOut.write("WRITE test a {} 1\n".format(i))
fileOut.close()
