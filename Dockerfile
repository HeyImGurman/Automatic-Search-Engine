FROM node:22-bookworm

RUN apt-get update \
    && apt-get install -y --no-install-recommends g++ \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY package*.json ./
RUN npm ci --omit=dev

COPY . .
RUN mkdir -p build \
    && g++ -std=c++17 -O2 -Wall -Wextra -I. -o build/autocomplete main.cpp Trie.cpp WordStore.cpp

ENV NODE_ENV=production
EXPOSE 3001
CMD ["npm", "run", "server"]
