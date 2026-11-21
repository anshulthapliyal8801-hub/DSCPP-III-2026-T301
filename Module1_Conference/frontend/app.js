// MUNSphere — Module 1 lab
// Every number shown here comes from the C code via the API:
// comparisons are counted inside conference_dsa.c and time is measured
// around the C call in Conference.cpp. Nothing is simulated in the browser.
// Admin only: the lab endpoints require an Admin login.
"use strict";

const LISTS = [
  { key: "conferences", type: "conference", title: "Conferences (insertion order)" },
  { key: "committees", type: "committee", title: "Committees (insertion order)" },
  { key: "sessions", type: "session", title: "Sessions (sorted by date, then start time)" },
];

const listsBox = document.getElementById("lists");
const reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)").matches;

async function loadLists() {
  try {
    const data = await Api.get("/api/dsa/module1/structure");
    listsBox.innerHTML = LISTS.map((l) => {
      const nodes = data[l.key];
      const chain = nodes.map((n) => `
        <div class="node" data-list="${l.type}" data-id="${UI.esc(n.id)}">
          <b>${UI.esc(n.id)}</b><small>${UI.esc(n.label)}</small>
        </div><span class="arrow" aria-hidden="true">→</span>`).join("");
      return `<div class="list-row">
        <h3>${UI.esc(l.title)}, ${nodes.length} node${nodes.length === 1 ? "" : "s"}</h3>
        <div class="chain"><span class="head-label">head</span>${chain}<span class="null">NULL</span></div>
      </div>`;
    }).join("");
  } catch (err) {
    listsBox.innerHTML = `<div class="banner">${UI.esc(err.message)}</div>`;
  }
}

function clearHighlights() {
  document.querySelectorAll(".node.visited, .node.found")
    .forEach((n) => n.classList.remove("visited", "found"));
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function runSearch() {
  const type = document.getElementById("search-type").value;
  const id = document.getElementById("search-id").value.trim().toUpperCase();
  const box = document.getElementById("search-result");
  if (!id) { box.innerHTML = `<p class="verdict-missing">Enter an ID to search for.</p>`; return; }

  await loadLists();  // show the list exactly as the search will see it
  clearHighlights();
  let r;
  try {
    r = await Api.get(`/api/dsa/module1/search?type=${encodeURIComponent(type)}&id=${encodeURIComponent(id)}`);
  } catch (err) {
    box.innerHTML = `<div class="banner">${UI.esc(err.message)}</div>`;
    return;
  }

  // Replay the path the C search actually took.
  for (let i = 0; i < r.path.length; i++) {
    const node = document.querySelector(`.node[data-list="${type}"][data-id="${CSS.escape(r.path[i])}"]`);
    if (!node) continue;
    const last = i === r.path.length - 1;
    node.classList.add(last && r.found ? "found" : "visited");
    node.scrollIntoView({ block: "nearest", inline: "nearest" });
    if (!reduceMotion) await sleep(220);
  }

  box.innerHTML = `
    <p class="${r.found ? "verdict-found" : "verdict-missing"}">
      ${r.found ? `Found ${UI.esc(r.target)}.` : `${UI.esc(r.target)} isn’t in the list.`}</p>
    <div class="result-stats">
      <div><span>Comparisons</span><b>${r.comparisons}</b></div>
      <div><span>List length</span><b>${r.listLength}</b></div>
      <div><span>Measured time</span><b>${r.elapsedNs.toLocaleString()} ns</b></div>
      <div><span>Complexity</span><b>${UI.esc(r.complexity)}</b></div>
    </div>
    <p class="sub">Path: ${r.path.length ? r.path.map(UI.esc).join(" → ") : "empty list, no nodes visited"}</p>
    <p class="sub">Times this small vary from run to run with CPU load; compare comparison counts, not nanoseconds.</p>`;
}

async function runNameSearch() {
  const q = document.getElementById("name-query").value.trim();
  const box = document.getElementById("name-result");
  try {
    const r = await Api.get(`/api/dsa/module1/search-name?q=${encodeURIComponent(q)}`);
    const matches = r.matches.map((c) =>
      `<li><strong>${UI.esc(c.abbreviation)}</strong> ${UI.esc(c.name)} <span class="id">${UI.esc(c.id)}</span></li>`).join("");
    box.innerHTML = `
      <div class="result-stats">
        <div><span>Nodes examined</span><b>${r.comparisons}</b></div>
        <div><span>Matches</span><b>${r.matches.length}</b></div>
        <div><span>Measured time</span><b>${r.elapsedNs.toLocaleString()} ns</b></div>
      </div>
      ${matches ? `<ul>${matches}</ul>` : `<p class="sub">No committee matches “${UI.esc(q)}”.</p>`}`;
  } catch (err) {
    box.innerHTML = `<div class="banner">${UI.esc(err.message)}</div>`;
  }
}

document.getElementById("reload").addEventListener("click", () => { clearHighlights(); loadLists(); });
document.getElementById("run-search").addEventListener("click", runSearch);
document.getElementById("search-id").addEventListener("keydown", (e) => { if (e.key === "Enter") runSearch(); });
document.getElementById("run-name").addEventListener("click", runNameSearch);
document.getElementById("name-query").addEventListener("keydown", (e) => { if (e.key === "Enter") runNameSearch(); });

Auth.requireRole("admin").then(loadLists);
