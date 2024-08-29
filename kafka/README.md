# Celte

EIP project about server meshing

## Utils Command

### Network related:

* **Run container:**

  ```
  docker-compose run --service-ports --rm serverstack /bin/bash
  ```
* **Test network connection:**

  ```
  echo "hello" | nc -u 127.0.0.1 8080
  ```
  ⚠️ **Note:** The current port is 8080. Use `lsof -i :8080` to see if a program is running on this port.
