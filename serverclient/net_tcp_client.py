import multiprocessing
import time
import socket
import ssl
import threading
import os


filenum = 3
def file_upload(fid):
    upsize = 0
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)#对每一个文件传输启用一个client
    s.connect(('127.0.0.1', 9999))# 建立连接:
    data = s.recv(1024)
    # print(s.recv(1024).decode('utf-8'))# 接收欢迎消息
    s.send(b'upload')

    filename = "file"+str(fid) # 拼接文件名字
    path =filename +".txt"
    filesize = str(os.path.getsize(path))

    print('begin to upload file %s to server,size : %s bytes' % (filename,filesize))
    s.send(filesize.encode('utf-8'))
    f = open(path,'rb')
    s.send(filename.encode('utf-8'))
    start = time.time()
    for line in f:
        s.send(line)
        upsize += len(line)
    data = s.recv(1024)
    if data.decode('utf-8') == 'ok':
        f.close()
        s.close()
        end = time.time()
        print('file %s upload finished ,cost %s s,upload %d bytes' % (filename,str(round(end - start, 2)),int(upsize)))

def file_download(fid):
    recsize = 0
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)#对每一个文件传输启用一个client
    sock.connect(('127.0.0.1', 9999))# 建立连接:
    data = sock.recv(1024)
    sock.send(b'download')
    sock.send(str(fid).encode('utf-8'))
    data = sock.recv(1024)
    filesize = int(data.decode('utf-8'))
    data = sock.recv(1024)
    filename = str(data.decode('utf-8'))
    print('begin to download from server,file name : %s,size : %d'%(filename,filesize))
    path = str(filename + "_c_receved" + ".txt")
    f = open(path,'wb')
    start = time.time()
    while recsize < filesize:
        data = sock.recv(1024)
        f.write(data)
        recsize += len(data)
        print('%s receving %d byte'% (filename,recsize))
    f.close()
    end = time.time()
    print('file %s upload finished ,cost %s s,download %d bytes' % (filename,str(round(end - start, 2)),int(recsize)))
    sock.close()


print('Try to upload file to server\n')

for i in range(1,filenum + 1):
    t = threading.Thread(target = file_upload,args = (i,))
    t.start()
    t.join()
print('\n')
print('Try to download file from server\n')
for i in range(1,filenum + 1):
    t = threading.Thread(target = file_download,args = (i,))
    t.start()
    t.join()
