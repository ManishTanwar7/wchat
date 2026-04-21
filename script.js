const API_BASE = "https://chatboot-k4o9.onrender.com";

function setStatus(message, isError = false) {
  const status = document.getElementById("status");
  status.textContent = message;
  status.style.color = isError ? "#ffd5d5" : "#d7ffe4";
}

function clearFields() {
  document.getElementById("sender").value = "";
  document.getElementById("receiver").value = "";
  document.getElementById("type").value = "text";
  document.getElementById("content").value = "";
  setStatus("Form cleared");
}

function escapeHtml(text) {
  return String(text)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

function messageHTML(msg) {
  const sender = escapeHtml(msg.sender || "");
  const receiver = escapeHtml(msg.receiver || "");
  const type = escapeHtml((msg.type || "text").toUpperCase());
  const content = escapeHtml(msg.content || "");
  const time = escapeHtml(msg.time || "");
  const isSent = sender.toLowerCase() === "me";

  return `
    <div class="message ${isSent ? "sent" : "received"}">
      <div class="meta">${sender} → ${receiver}</div>
      <div class="type">${type}</div>
      <div class="text">${content}</div>
      <div class="time">${time}</div>
    </div>
  `;
}

function renderMessages(messages) {
  const chatBody = document.getElementById("chatBody");

  if (!messages || messages.length === 0) {
    chatBody.innerHTML = `<div class="empty-chat">No messages yet.</div>`;
    return;
  }

  chatBody.innerHTML = messages.map(messageHTML).join("");
  chatBody.scrollTop = chatBody.scrollHeight;
}

async function loadMessages() {
  try {
    setStatus("Loading messages...");
    const response = await fetch(`${API_BASE}/api/messages`);
    const data = await response.json();

    if (!response.ok || data.status !== "success") {
      throw new Error(data.message || "Failed to load messages");
    }

    renderMessages(data.messages || []);
    setStatus("Messages loaded successfully");
  } catch (error) {
    setStatus(error.message, true);
  }
}

async function sendMessage() {
  const sender = document.getElementById("sender").value.trim();
  const receiver = document.getElementById("receiver").value.trim();
  const type = document.getElementById("type").value;
  const content = document.getElementById("content").value.trim();

  if (!sender || !receiver || !type || !content) {
    setStatus("Please fill all fields", true);
    return;
  }

  try {
    setStatus("Sending message...");

    const response = await fetch(`${API_BASE}/api/messages`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json"
      },
      body: JSON.stringify({ sender, receiver, type, content })
    });

    const data = await response.json();

    if (!response.ok || data.status !== "success") {
      throw new Error(data.message || "Failed to send message");
    }

    renderMessages(data.messages || []);
    document.getElementById("content").value = "";
    setStatus("Message sent successfully");
  } catch (error) {
    setStatus(error.message, true);
  }
}

window.onload = loadMessages;
