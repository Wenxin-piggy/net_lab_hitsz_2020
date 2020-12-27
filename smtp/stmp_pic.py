from socket import *
import base64

subject = "I love u yea"
contenttype = "multipart/alternative;"
msg = "I don't love computer networks!I love you baby:)"
endmsg = "\r\n.\r\n"
boundary="----=_Part_218429_202270086.1495197432753"


mailserver = "smtp.163.com"  #选择服务器
mailsender = "shen_wenxin@163.com"
mailuser = 'shen_wenxin'
password = 'UOVFERDQGXXGVXCK'

# mailreciever = "shenwenxin0510@gmail.com"
mailreciever = "shen_wenxin@qq.com"


# Create socket called clientSocket and establish a TCP connection with mailserver
clientSocket = socket(AF_INET, SOCK_STREAM)
clientSocket.connect((mailserver, 25))

recv = clientSocket.recv(1024).decode()
print(recv)
if recv[:3] != '220':
    print('220 reply not received from server.')

# Send HELO command and print server response.
hello_command = 'HELO Alice\r\n'
clientSocket.send(hello_command.encode())
recv1 = clientSocket.recv(1024).decode()
print(recv1)
if recv1[:3] != '250':
    print('250 reply not received from server.')

# Auth
# login
logincommand = 'Auth login\r\n'
while True:
    clientSocket.send(logincommand.encode())
    recv = clientSocket.recv(1024).decode()
    if recv[:3] == '334':
        print('login send')
        break
    else:
        print('login : 334 reply not received from server.')

# base64 username then send
username = base64.b64encode(mailsender.encode()) + b'\r\n'
while True:
    clientSocket.send(username)
    recv = clientSocket.recv(1024).decode()
    if recv[:3] == '334':
        print('username send')
        break
    else:
        print('username : 334 reply not received from server.')

# base64 password then send
password64 = base64.b64encode(password.encode()) + b'\r\n'
while True:
    clientSocket.send(password64)
    recv = clientSocket.recv(1024).decode()
    if recv[:3] == '235':
        print('password send')
        break
    else:
        print('password : 225 reply not received from server.')

# Send MAIL FROM command and print server response.
mfc_command = 'MAIL FROM: <'+ mailsender + '>\r\n'
while True:
    clientSocket.send(mfc_command.encode())
    recv = clientSocket.recv(1024).decode()
    if recv[:3] == '250':
        print('mfc_command send')
        break
    else:
        print('mfc_command : 250 reply not received from server.')

# Send RCPT TO command and print server response.
rtc_command = 'RCPT TO: <'+ mailreciever + '>\r\n'
while True:
    clientSocket.send(rtc_command.encode())
    recv = clientSocket.recv(1024).decode()
    print(recv)
    if recv[:3] == '250':
        print('rtc_command send')
        break
    else:
        print('rtc_command : 250 reply not received from server.')

# Send DATA command and print server response.
data_command = 'DATA\r\n'
while True:
    clientSocket.send(data_command.encode())
    recv = clientSocket.recv(1024).decode()
    print(recv)
    if recv[:3] == '354':
        print('data_command send')
        break
    else:
        print('data_command : 354 reply not received from server.')

# Send message data.
message = 'from:' + mailsender + '\r\n'
message += 'to:' + mailreciever + '\r\n'
message += 'subject:' + subject + '\r\n'
message += 'Content-Type:' + contenttype + '\r\n'
message += '\tboundary='+boundary + '\r\n'
message += 'MIME-Version: 1.0' + '\r\n'
# message += '\r\n' + msg

# Send Attachment
message += '--'+boundary+ '\r\n'
message += 'Content-Transfer-Encoding: base64\r\nContent-Type: image/jpeg;name="test.jpeg"\r\nContent-Disposition: attachment;filename="test.jpeg"\r\n\r\n'
clientSocket.sendall(message.encode())
with open("test.jpeg","rb") as f:
    dstream = base64.b64encode(f.read())
    # print(dstream.decode())
clientSocket.sendall(dstream)
message = '\r\n'

message += '--'+boundary+ '\r\n'
message += 'Content-Type: text/plain; charset=GBK\r\n'
message += 'Content-Transfer-Encoding: base64\r\n'
clientSocket.sendall(message.encode())
message += 'I love u \r\n\r\n'
clientSocket.send(message.encode())
dstream = b':)'
clientSocket.sendall(dstream)
message = '\r\n'
clientSocket.sendall(message.encode())

# Message ends with a single period.
while True:
    clientSocket.sendall(endmsg.encode())
    recv = clientSocket.recv(1024).decode()
    if recv[:3] == '250':
        print('endmsg send')
        break
    else:
        print('endmsg : 250 reply not received from server.')


# Send QUIT command and get server response.
quit_command = 'QUIT\r\n'
while True:
    clientSocket.send(quit_command.encode())
    recv = clientSocket.recv(1024).decode()
    if recv[:3] == '221':
        print('quit send')
        break
    else:
        print('quit : 221 reply not received from server.')


