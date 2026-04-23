const API_BASE = "https://wchat-v4sn.onrender.com";

let autoRefreshInterval = null;

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
  document.getElementById("chatBody").innerHTML = `<div class="empty-chat">No messages yet.</div>`;
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

function messageHTML(msg, currentSender) {
  const sender = escapeHtml(msg.sender || "");
  const receiver = escapeHtml(msg.receiver || "");
  const type = escapeHtml((msg.type || "text").toUpperCase());
  const content = escapeHtml(msg.content || "");
  const time = escapeHtml(msg.time || "");

  const isSent = sender.toLowerCase() === currentSender.toLowerCase();

  return `
    <div class="message ${isSent ? "sent" : "received"}">
      <div class="meta">${sender} → ${receiver}</div>
      <div class="type">${type}</div>
      <div class="text">${content}</div>
      <div class="time">${time}</div>
    </div>
  `;
}

function renderMessages(messages, currentSender) {
  const chatBody = document.getElementById("chatBody");

  if (!messages || messages.length === 0) {
    chatBody.innerHTML = `<div class="empty-chat">No messages yet.</div>`;
    return;
  }

  chatBody.innerHTML = messages.map(msg => messageHTML(msg, currentSender)).join("");
  chatBody.scrollTop = chatBody.scrollHeight;
}

async function loadMessages(showStatus = true) {
  const sender = document.getElementById("sender").value.trim();
  const receiver = document.getElementById("receiver").value.trim();

  if (!sender || !receiver) {
    if (showStatus) setStatus("Enter sender and receiver first", true);
    return;
  }

  try {
    if (showStatus) setStatus("Loading chat...");

    const response = await fetch(
      `${API_BASE}/api/messages?sender=${encodeURIComponent(sender)}&receiver=${encodeURIComponent(receiver)}`
    );

    const data = await response.json();

    if (!response.ok || data.status !== "success") {
      throw new Error(data.message || "Failed to load messages");
    }

    renderMessages(data.messages || [], sender);

    if (showStatus) setStatus("Chat loaded");
  } catch (error) {
    if (showStatus) setStatus(error.message, true);
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

    document.getElementById("content").value = "";
    renderMessages(data.messages || [], sender);
    setStatus("Message sent successfully");
  } catch (error) {
    setStatus(error.message, true);
  }
}

function startAutoRefresh() {
  if (autoRefreshInterval) {
    clearInterval(autoRefreshInterval);
  }

  autoRefreshInterval = setInterval(() => {
    const sender = document.getElementById("sender").value.trim();
    const receiver = document.getElementById("receiver").value.trim();

    if (sender && receiver) {
      loadMessages(false);
    }
  }, 2000);
}

window.onload = function () {
  startAutoRefresh();
};
