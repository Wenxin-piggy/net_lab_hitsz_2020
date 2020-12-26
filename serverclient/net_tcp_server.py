import threading
import time
import socket
import ssl
import os


def tcplink(sock, addr):
    # 按要求获取线程id
    t = threading.currentThread()
    print('thread id : %d' % t.ident)
    print('thread name : %s'% t.getName())
    print('Accept new connection from %s:%s...' % addr)
    sock.send(b'Welcome!')
    op = sock.recv(1024)
    if op.decode('utf-8') == 'upload':
        recsize = 0
        data = sock.recv(1024)
        filesize = int(data.decode('utf-8'))
        data = sock.recv(1024)
        filename = str(data.decode('utf-8'))
        print('begin to revieved : %s,size : %d'%(filename,filesize))
        path = str(filename + "_s_receved" + ".txt")
        f = open(path,'wb')
        while recsize < filesize:
            data = sock.recv(1024)
            f.write(data)
            recsize += len(data)
            print('%s receving %d byte'% (filename,recsize))
        f.close()
        print('finished,%s revieved %s bytes'%(filename,recsize))
        sock.send(b'ok')
        sock.close()
    elif op.decode('utf-8') == 'download':  
        data = sock.recv(1024)
        fid = int(data.decode('utf-8'))
        filename = "file" + str(fid) + "_s_receved"
        path = filename + ".txt"
        filesize = str(os.path.getsize(path))
        print('begin to upload file %s to client,size : %s bytes' % (filename,filesize))
        sock.send(filesize.encode('utf-8'))
        sock.send(filename.encode('utf-8'))
        f = open(path,'rb')
        start = time.time()
        for line in f:
            sock.send(line)
        f.close()
        
        end = time.time()
        print('file %s upload finished ,cost %s s,upload %d bytes' % (filename,str(round(end - start, 2)),int(filesize)))
        sock.close()
        print ('Connection from %s:%s closed.' % addr)

# 首先，创建一个基于IPv4和TCP协议的Socket
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# 绑定监听的地址和端口
s.bind(('127.0.0.1', 9999))

# 调用litsen方法开始监听端口
s.listen(5)
print('Waiting for connection...')

while True:
    # 接受一个新连接:
    sock, addr = s.accept()
    # 创建新线程来处理TCP连接:
    t = threading.Thread(target=tcplink, args=(sock, addr))
    t.start()