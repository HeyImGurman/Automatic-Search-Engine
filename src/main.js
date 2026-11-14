import "./style.css";

const input = document.querySelector("#search-input");
const connection = document.querySelector("#connection");
const heading = document.querySelector("#results-heading");
const list = document.querySelector("#results-list");
const emptyState = document.querySelector("#empty-state");

const configuredUrl = import.meta.env.VITE_WS_URL;
const socketUrl = configuredUrl || (import.meta.env.DEV ? "ws://localhost:3001" : "");
let socket;
let requestId = 0;
let latestRequestId = 0;
let debounceTimer;

function setConnection(message, state) {
  connection.textContent = message;
  connection.dataset.state = state;
}

function clearResults(message = "") {
  heading.textContent = "";
  list.replaceChildren();
  emptyState.textContent = message;
  emptyState.hidden = !message;
}

function connect() {
  if (!socketUrl) {
    setConnection("Set VITE_WS_URL to your deployed WebSocket service.", "error");
    return;
  }

  socket = new WebSocket(socketUrl);
  socket.addEventListener("open", () => {
    setConnection("Connected to the live search engine", "connected");
    search(input.value);
  });
  socket.addEventListener("close", () => {
    setConnection("Search service disconnected. Refresh to reconnect.", "error");
  });
  socket.addEventListener("error", () => {
    setConnection("Could not reach the search service.", "error");
  });
  socket.addEventListener("message", (event) => {
    let response;
    try {
      response = JSON.parse(event.data);
    } catch {
      setConnection("Received an invalid response from the search service.", "error");
      return;
    }
    if (response.id !== latestRequestId) return;

    if (response.type === "error") {
      clearResults(response.message || "Search failed.");
      return;
    }
    renderResults(response);
  });
}

function search(prefix) {
  const normalized = prefix.trim();
  if (!normalized) {
    clearResults("Start typing to explore the dictionary.");
    return;
  }
  if (!socket || socket.readyState !== WebSocket.OPEN) {
    clearResults("Waiting for the search service…");
    return;
  }

  const id = ++requestId;
  latestRequestId = id;
  socket.send(JSON.stringify({ type: "search", id, prefix: normalized }));
}

function renderResults(response) {
  const items = Array.isArray(response.items) ? response.items : [];
  list.replaceChildren();
  if (items.length === 0) {
    heading.textContent = "";
    emptyState.textContent = "No matches yet. Try another prefix.";
    emptyState.hidden = false;
    return;
  }

  heading.textContent = `${items.length} suggestions · ${response.totalMatches} matches`;
  emptyState.hidden = true;
  for (const [index, item] of items.entries()) {
    const row = document.createElement("li");
    row.className = "result-row";

    const rank = document.createElement("span");
    rank.className = "rank";
    rank.textContent = String(index + 1).padStart(2, "0");

    const word = document.createElement("span");
    word.className = "word";
    word.textContent = item.word;

    const frequency = document.createElement("span");
    frequency.className = "frequency";
    frequency.textContent = `${item.frequency} searches`;

    row.append(rank, word, frequency);
    list.append(row);
  }
}

input.addEventListener("input", () => {
  window.clearTimeout(debounceTimer);
  debounceTimer = window.setTimeout(() => search(input.value), 90);
});

clearResults("Start typing to explore the dictionary.");
connect();
