
TARGET=zs

$(TARGET): zs.o
	g++ $^ -o $@ -lzstd

clean:
	rm *.o
	rm $(TARGET)
	rm __*
