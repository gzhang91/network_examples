EXE=test

$(EXE): test.cpp poller.cpp timeout_manager.cpp timeout_object.cpp
	g++ -g -o $@ $^

clean:
	- rm -rf *.o
	- rm -rf $(EXE)
