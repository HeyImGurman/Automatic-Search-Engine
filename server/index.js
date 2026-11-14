import { spawn } from "node:child_process";
import { createServer } from "node:http";
import { resolve, join } from "node:path";
import { createInterface } from "node:readline";
import { WebSocketServer, WebSocket } from "ws";

const port = Number.parseInt(process.env.PORT || "3001", 10);
const enginePath =
  process.env.SEARCH_ENGINE_BIN ||
  join(process.cwd(), "build", process.platform === "win32" ? "autocomplete.exe" : "autocomplete");
const wordsPath = resolve(process.env.WORDS_FILE || "data/words.txt");
const maxQueuedSearches = 100;
const allowedOrigins = new Set(
  (process.env.ALLOWED_ORIGINS || "")
    .split(",")
    .map((origin) => origin.trim())
    .filter(Boolean),
);

const engine = spawn(enginePath, ["--stdio", wordsPath], {
  stdio: ["pipe", "pipe", "inherit"],
});
let engineReady = false;
let activeJob = null;
const jobs = [];

engine.once("spawn", () => {
  engineReady = true;
  console.log(`Search engine started with ${wordsPath}`);
});

function send(job, payload) {
  if (job.socket.readyState === WebSocket.OPEN) {
    job.socket.send(JSON.stringify({ ...payload, id: job.id }));
  }
}

function failEngine(error) {
  engineReady = false;
  const message = error instanceof Error ? error.message : String(error);
  console.error(`Search engine stopped: ${message}`);
  const failedJobs = activeJob ? [activeJob, ...jobs.splice(0)] : jobs.splice(0);
  activeJob = null;
  for (const job of failedJobs) {
    send(job, { type: "error", message: "Search engine is unavailable." });
  }
}

function processNextJob() {
  if (!engineReady || activeJob || jobs.length === 0) return;
  activeJob = jobs.shift();
  engine.stdin.write(`${activeJob.prefix}\n`, (error) => {
    if (error) failEngine(error);
  });
}

const engineOutput = createInterface({ input: engine.stdout });
engineOutput.on("line", (line) => {
  const job = activeJob;
  activeJob = null;
  if (!job) {
    console.error("Search engine returned a response without a matching request.");
    return;
  }

  try {
    const result = JSON.parse(line);
    if (result.error) {
      send(job, { type: "error", message: "Enter a valid prefix (up to 64 characters)." });
    } else {
      send(job, { type: "results", ...result });
    }
  } catch (error) {
    console.error("Could not parse search engine response:", error);
    send(job, { type: "error", message: "Search engine returned an invalid response." });
  }
  processNextJob();
});

engine.on("error", failEngine);
engine.on("exit", (code, signal) => {
  failEngine(`process exited (${code ?? signal ?? "unknown"})`);
});

const httpServer = createServer((request, response) => {
  if (request.url === "/health") {
    const healthy = engineReady;
    response.writeHead(healthy ? 200 : 503, { "content-type": "application/json" });
    response.end(JSON.stringify({ status: healthy ? "ok" : "unavailable" }));
    return;
  }
  response.writeHead(404);
  response.end("Not found");
});

const websocketServer = new WebSocketServer({
  server: httpServer,
  maxPayload: 4096,
  verifyClient: ({ origin }, done) => {
    if (origin && allowedOrigins.size > 0 && !allowedOrigins.has(origin)) {
      done(false, 403, "Origin not allowed");
      return;
    }
    done(true);
  },
});

websocketServer.on("connection", (socket) => {
  socket.on("message", (data, isBinary) => {
    if (isBinary) {
      send({ socket }, { type: "error", message: "Only text search requests are supported." });
      return;
    }

    let request;
    try {
      request = JSON.parse(data.toString());
    } catch {
      send({ socket }, { type: "error", message: "Send a valid JSON search request." });
      return;
    }

    const id =
      request !== null && typeof request === "object" && !Array.isArray(request)
        ? request.id
        : undefined;
    if (
      request === null ||
      typeof request !== "object" ||
      Array.isArray(request) ||
      request.type !== "search" ||
      typeof request.prefix !== "string"
    ) {
      send({ socket, id }, { type: "error", message: "Expected a search request." });
      return;
    }
    if (!/^[A-Za-z0-9 '-]{1,64}$/.test(request.prefix)) {
      send({ socket, id }, { type: "error", message: "Enter a valid prefix (up to 64 characters)." });
      return;
    }
    if (!engineReady) {
      send({ socket, id }, { type: "error", message: "Search engine is unavailable." });
      return;
    }
    if (jobs.length >= maxQueuedSearches) {
      send({ socket, id }, { type: "error", message: "Search is busy. Try again shortly." });
      return;
    }

    jobs.push({ socket, id, prefix: request.prefix });
    processNextJob();
  });
});

httpServer.listen(port, "0.0.0.0", () => {
  console.log(`WebSocket search service listening on port ${port}`);
});

for (const signal of ["SIGINT", "SIGTERM"]) {
  process.on(signal, () => {
    websocketServer.close();
    httpServer.close(() => {
      engine.kill(signal);
      process.exit(0);
    });
  });
}
