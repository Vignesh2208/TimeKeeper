
import threading
import time



def daemon():
    print 'daemon Starting'
    time.sleep(0.2)
    print 'daemon Exiting'


def non_daemon():
    print 'ndaemon Starting'
    print 'ndaemon Exiting'


print "Main Starting"

d = threading.Thread(name='daemon', target=daemon)

t = threading.Thread(name='non-daemon', target=non_daemon)

d.start()
t.start()

d.join()
t.join()

print "Main Exiting"
