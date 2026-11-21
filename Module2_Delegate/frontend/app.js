// MUNSphere — Module 2 lab
// Everything shown comes from delegate_dsa.c through the API: bucket
// contents, load factor, rehash count, the BST shape and traversals, and
// comparison counts. Nothing is simulated in the browser. Admin only.
"use strict";

const esc = UI.esc;
const $ = (id) => document.getElementById(id);
let nameOf = {};

function stat(label, value) { return `<div><span>${esc(label)}</span><b>${value}</b></div>`; }

async function loadHash() {
  const h = await Api.get("/api/dsa/module2/hash");
  const pct = Math.min(100, Math.round((h.loadFactor / h.maxLoadFactor) * 100));
  $("hash-stats").innerHTML = stat("Delegates", h.size) + stat("Buckets", h.bucketCount) +
    stat("Buckets in use", h.usedBuckets) + stat("Longest chain", h.longestChain) + stat("Times resized", h.rehashes) +
    `<div><span>Load factor (limit ${h.maxLoadFactor})</span><b>${h.loadFactor.toFixed(3)}</b>
      <div class="load-bar" aria-hidden="true"><span data-w="${pct}"></span></div></div>`;
  document.querySelectorAll(".load-bar span").forEach((s) => { s.style.width = `${s.dataset.w}%`; });
  $("hash-rule").textContent = `${h.hashFunction}. ${h.collisionHandling}. Highlighted rows hold more than one delegate: a collision.`;
  $("buckets").innerHTML = h.size === 0
    ? `<p class="sub">No delegates yet. The table starts with 8 buckets and doubles when the load factor would pass ${h.maxLoadFactor}.</p>`
    : h.buckets.map((b) => `<div class="bucket${b.length > 1 ? " collision" : ""}"><span class="idx">[${b.index}]</span>${
      b.ids.length ? b.ids.map((id) => `<span class="cell">${esc(id)}</span><span class="arrow" aria-hidden="true">→</span>`).join("") + `<span class="null">NULL</span>`
        : `<span class="empty-b">empty</span>`}</div>`).join("");
}

async function loadTree() {
  const t = await Api.get("/api/dsa/module2/bst");
  nameOf = {};
  t.nodes.forEach((n) => { nameOf[n.id] = n.name; });
  $("bst-stats").innerHTML = stat("Nodes", t.nodeCount) + stat("Height", t.height) +
    stat("Minimum (leftmost)", t.min ? esc(nameOf[t.min] || t.min) : "none") +
    stat("Maximum (rightmost)", t.max ? esc(nameOf[t.max] || t.max) : "none");
  drawTree(t);
  const line = (ids) => ids.length ? ids.map((id) => esc(`${nameOf[id] || id}`)).join(" → ") : "empty tree";
  $("traversals").innerHTML = `<dt>In-order</dt><dd>${line(t.inorder)}</dd>
    <dt>Pre-order</dt><dd>${line(t.preorder)}</dd><dt>Post-order</dt><dd>${line(t.postorder)}</dd>`;
}

// Layout: x = the node's position in the in-order traversal (so the tree
// reads left to right alphabetically), y = depth from the root.
function drawTree(t) {
  const svg = $("tree");
  const NS = "http://www.w3.org/2000/svg";
  svg.innerHTML = "";
  if (!t.nodes.length) { svg.setAttribute("width", 300); svg.setAttribute("height", 40); return; }
  const order = {};
  t.inorder.forEach((id, i) => { order[id] = i; });
  const depth = t.nodes.map(() => 0);
  t.nodes.forEach((n, i) => { if (n.parent >= 0) depth[i] = depth[n.parent] + 1; });
  const W = 96, H = 64, BOX = 84;
  const width = Math.max(300, t.inorder.length * W + 20), height = (Math.max(...depth) + 1) * H + 20;
  svg.setAttribute("width", width); svg.setAttribute("height", height);
  svg.setAttribute("viewBox", `0 0 ${width} ${height}`);
  const pos = t.nodes.map((n, i) => ({ x: 10 + order[n.id] * W + W / 2, y: 14 + depth[i] * H + 18 }));
  t.nodes.forEach((n, i) => {
    if (n.parent < 0) return;
    const l = document.createElementNS(NS, "line");
    l.setAttribute("x1", pos[n.parent].x); l.setAttribute("y1", pos[n.parent].y + 16);
    l.setAttribute("x2", pos[i].x); l.setAttribute("y2", pos[i].y - 16);
    svg.appendChild(l);
  });
  t.nodes.forEach((n, i) => {
    const g = document.createElementNS(NS, "g");
    if (n.id === t.min) g.setAttribute("class", "min");
    if (n.id === t.max) g.setAttribute("class", "max");
    const r = document.createElementNS(NS, "rect");
    r.setAttribute("x", pos[i].x - BOX / 2); r.setAttribute("y", pos[i].y - 16);
    r.setAttribute("width", BOX); r.setAttribute("height", 32); r.setAttribute("rx", 4);
    const name = document.createElementNS(NS, "text");
    name.setAttribute("x", pos[i].x); name.setAttribute("y", pos[i].y - 1);
    name.textContent = n.name.length > 12 ? `${n.name.slice(0, 11)}…` : n.name;
    const id = document.createElementNS(NS, "text");
    id.setAttribute("x", pos[i].x); id.setAttribute("y", pos[i].y + 12); id.setAttribute("class", "nid");
    id.textContent = n.id;
    const title = document.createElementNS(NS, "title");
    title.textContent = `${n.name} (${n.id})`;
    g.append(r, name, id, title);
    svg.appendChild(g);
  });
}

async function compare() {
  const id = $("key-id").value.trim().toUpperCase();
  const name = $("key-name").value.trim() || (nameOf[id] || "");
  const box = $("compare-result");
  if (!id && !name) { box.innerHTML = `<p class="verdict-missing">Enter a delegate ID, a name, or both.</p>`; return; }
  const runs = [];
  if (id) for (const m of ["hash", "linear", "binary"]) runs.push(Api.get(`/api/dsa/module2/search?method=${m}&key=${encodeURIComponent(id)}`));
  if (name) runs.push(Api.get(`/api/dsa/module2/search?method=bst&key=${encodeURIComponent(name)}`));
  try {
    const results = await Promise.all(runs);
    box.innerHTML = `<div class="table-wrap"><table>
      <thead><tr><th>Search</th><th>Looked for</th><th>Found</th><th class="num">Comparisons</th><th class="num">Time</th><th>Complexity</th></tr></thead>
      <tbody>${results.map((r) => `<tr><td><strong>${esc(r.algorithm)}</strong>
        <span class="path">Path: ${r.path.map(esc).join(" → ") || "none"}${r.pathTruncated ? " …" : ""}</span></td>
        <td>${esc(r.key)}</td><td>${r.found ? esc(r.result) : "Not found"}</td>
        <td class="num">${r.comparisons}</td><td class="num">${r.elapsedNs.toLocaleString()} ns</td>
        <td>${esc(r.complexity)}</td></tr>`).join("")}</tbody></table></div>
      <p class="table-note">n = ${results[0].n}. With only a few delegates every method is fast; the comparison
        counts show how each one grows. Times this small vary with CPU load, so compare the counts.</p>`;
  } catch (err) {
    box.innerHTML = `<div class="banner">${esc(err.message)}</div>`;
  }
}

async function loadAll() {
  try { await Promise.all([loadHash(), loadTree()]); }
  catch (err) { $("buckets").innerHTML = `<div class="banner">${esc(err.message)}</div>`; }
}

$("reload").addEventListener("click", loadAll);
$("compare").addEventListener("click", compare);
["key-id", "key-name"].forEach((k) => $(k).addEventListener("keydown", (e) => { if (e.key === "Enter") compare(); }));
Auth.requireRole("admin").then(loadAll);
