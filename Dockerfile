FROM ubuntu:22.04

RUN apt-get update && apt-get install -y g++ && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY server.cpp .

RUN g++ server.cpp -o server

EXPOSE 10000

CMD ["./server"]
