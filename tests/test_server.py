import http.client
import socket
from concurrent.futures import ThreadPoolExecutor, as_completed
import time

HOST = "127.0.0.1"
PORT = 8080

def test_hello():
    conn = http.client.HTTPConnection(HOST, PORT, timeout=3)

    try:
        conn.request("GET", "/hello")

        response = conn.getresponse()
        body = response.read().decode()

        assert response.status == 200
        assert body == "Welcome to my HTTP server!"

        print("PASS: GET /hello")
    finally:
        conn.close()


def test_not_found():
    conn = http.client.HTTPConnection(HOST, PORT, timeout=3)

    try:
        conn.request("GET", "/does-not-exist")

        response = conn.getresponse()
        body = response.read().decode()

        assert response.status == 404
        assert body == "404 Not Found"

        print("PASS: GET /does-not-exist")
    finally:
        conn.close()

def test_method_not_allowed():
    conn = http.client.HTTPConnection(HOST, PORT, timeout=3)

    try:
        conn.request("POST", "/hello")

        response = conn.getresponse()
        body = response.read().decode()

        assert response.status == 405
        assert response.getheader("Allow") == "GET"
        assert body == "Method Not Allowed"

        print("PASS: POST /hello returns 405")
    finally:
        conn.close()

def test_bad_request():
    with socket.create_connection((HOST, PORT), timeout=3) as sock:
        sock.sendall(b"GET hello HTTP/1.1\r\n\r\n")

        response = sock.recv(4096)

        assert b"HTTP/1.1 400 Bad Request" in response

        print("PASS: Malformed request returns 400")

def test_concurrent_clients():
    total_requests = 100

    def send_request(request_id):
        conn = http.client.HTTPConnection(HOST, PORT, timeout=10)

        try:
            conn.request("GET", "/hello")
            response = conn.getresponse()
            body = response.read().decode()

            return (
                response.status == 200
                and body == "Welcome to my HTTP server!"
            )
        finally:
            conn.close()

    start = time.perf_counter()

    with ThreadPoolExecutor(max_workers=10) as executor:
        futures = [
            executor.submit(send_request, i)
            for i in range(total_requests)
        ]

        results = [future.result() for future in as_completed(futures)]

    elapsed = time.perf_counter() - start

    assert all(results), "Some concurrent requests failed"

    print(f"PASS: {total_requests} concurrent-test requests")
    print(f"Completed in {elapsed:.3f} seconds")

if __name__ == "__main__":
    test_hello()
    test_not_found()
    test_method_not_allowed()
    test_bad_request()
    test_concurrent_clients()