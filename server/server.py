import socket

addr = ""
port = 25566
buff_size = 1024
timout = 5

with socket.create_server(address=(addr,port),family=socket.AF_INET) as s:
    s.listen()
    print(f"listening at {addr}:{port}")

    while 1==1:
        conn, addr = s.accept()
        conn.settimeout(timout)
        print(f"new connection at {addr}")
        while 1==1:
            try:
                data = conn.recv(buff_size)
                if not data:
                    print("connection closed by client")
                    conn.close()
                    break
                print(f"data recieved: {data}")
                conn.send(b"C")
            except TimeoutError:
                print("connection timed out")
                conn.close()
                break
            except Exception as e:
                print(f"unknown connection error: {e.strerror}")
                conn.close()
                break
