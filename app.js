// MUNSphere Dashboard Script

// ===== ROLES =====
// delegate : read-only (can raise POI / POO, withdraw own pending point)
// eb       : Executive Board - can edit data, and is the only role that marks speeches
// oc       : Organising Committee - can edit data, and is the only role that can
//            change the Main Marksheet and rule on POI / POO
const ROLES = {
    delegate: { label: 'Delegate',             canEdit: false },
    eb:       { label: 'Executive Board',      canEdit: true  },
    oc:       { label: 'Organising Committee', canEdit: true  }
};

let currentRole = 'delegate';
let currentUsername = '';

function canEdit() {
    return ROLES[currentRole] && ROLES[currentRole].canEdit;
}

// Guard used by every function that changes conference data
function requireEdit() {
    if (!canEdit()) {
        alert('View-only access: your role (' + ROLES[currentRole].label + ') cannot edit data.');
        return false;
    }
    return true;
}

// Speech marking is for the Executive Board only
function isEB() {
    return currentRole === 'eb';
}

function requireEB() {
    if (!isEB()) {
        alert('Speech marking is restricted to Executive Board members.');
        return false;
    }
    return true;
}

// Main Marksheet and POI/POO rulings are for the Organising Committee only
function isOC() {
    return currentRole === 'oc';
}

function requireOC() {
    if (!isOC()) {
        alert('Only the Organising Committee can do this.');
        return false;
    }
    return true;
}

// Escape user-entered text before putting it into innerHTML
function esc(value) {
    return String(value).replace(/[&<>"']/g, ch => ({
        '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
    }[ch]));
}

// Next free ID (safe even after deletions)
function nextId(prefix, list, pad) {
    const max = list.reduce((m, item) => {
        const n = parseInt(String(item.id).replace(prefix, ''), 10);
        return isNaN(n) ? m : Math.max(m, n);
    }, 0);
    return prefix + String(max + 1).padStart(pad, '0');
}

// Store data locally
let appData = {
    delegates: [],
    committees: [],
    sessions: []
};

// Points of Information / Order (kept under its own key so existing saved data still loads)
let points = [];

// Check authentication
window.addEventListener('load', function () {
    const role = localStorage.getItem('role');
    if (localStorage.getItem('loggedIn') !== 'true' || !ROLES[role]) {
        localStorage.removeItem('loggedIn');
        window.location.href = 'index.html';
        return;
    }

    currentRole = role;
    currentUsername = localStorage.getItem('username') || 'User';

    // Set current user + role UI
    document.getElementById('currentUser').textContent = currentUsername;
    const badge = document.getElementById('roleBadge');
    badge.textContent = ROLES[role].label;
    badge.classList.add('role-' + role);

    document.body.classList.add(canEdit() ? 'can-edit' : 'read-only');
    document.body.classList.add('role-' + role);
    document.getElementById('readOnlyBanner').style.display = canEdit() ? 'none' : 'block';

    // Sidebar navigation
    document.querySelectorAll('.nav-item').forEach(item => {
        item.addEventListener('click', function (e) {
            e.preventDefault();
            showSection(this.dataset.section);
        });
    });

    // Load data from localStorage or initialize
    loadData();
    loadPoints();
    loadSpeeches();

    // Display initial dashboard section
    showSection('dashboard', true);

    // Single orchestrated entrance: stat cards sweep in with motion blur
    document.querySelectorAll('#dashboard .stat-card').forEach((card, i) => {
        motionBlur(card, { dir: 'x', distance: -90, duration: 650, maxBlur: 14, delay: 90 * i });
    });
});

// Load sample data or from localStorage
function loadData() {
    const saved = localStorage.getItem('munsphere-data');

    if (saved) {
        appData = JSON.parse(saved);
    } else {
        initializeSampleData();
    }

    updateDashboard();
    updateDelegatesTable();
    updateCommitteesTable();
    updateSessionsTable();
}

// Initialize sample data
function initializeSampleData() {
    appData.delegates = [
        { id: 'D001', name: 'Aarav Sharma', country: 'India', committee: 'UNGA', score: 85 },
        { id: 'D002', name: 'John Smith', country: 'United States', committee: 'UNGA', score: 78 },
        { id: 'D003', name: 'Emma Wilson', country: 'United Kingdom', committee: 'UNHRC', score: 82 },
        { id: 'D004', name: 'Pierre Dubois', country: 'France', committee: 'UNHRC', score: 76 },
        { id: 'D005', name: 'Klaus Mueller', country: 'Germany', committee: 'AIPPM', score: 79 },
        { id: 'D006', name: 'Yuki Tanaka', country: 'Japan', committee: 'AIPPM', score: 88 },
        { id: 'D007', name: 'Carlos Rodriguez', country: 'Brazil', committee: 'WHO', score: 74 },
        { id: 'D008', name: 'Wei Chen', country: 'China', committee: 'WHO', score: 72 }
    ];

    appData.committees = [
        { id: 'COM001', name: 'UNGA', capacity: 20, members: 2 },
        { id: 'COM002', name: 'UNHRC', capacity: 15, members: 2 },
        { id: 'COM003', name: 'AIPPM', capacity: 18, members: 2 },
        { id: 'COM004', name: 'WHO', capacity: 12, members: 2 }
    ];

    appData.sessions = [
        { id: 'SESS001', name: 'General Debate', date: '2026-10-15', status: 'Planning' },
        { id: 'SESS002', name: 'Formal Voting', date: '2026-10-16', status: 'Planning' },
        { id: 'SESS003', name: 'Committee Meeting', date: '2026-10-17', status: 'Planning' }
    ];

    saveData();
}

// Save data to localStorage
function saveData() {
    localStorage.setItem('munsphere-data', JSON.stringify(appData));
}

// Switch between sections
function showSection(sectionId, silent) {
    if (sectionId === 'speeches' && !isEB()) return;
    // Hide all sections
    document.querySelectorAll('.content-section').forEach(section => section.style.display = 'none');

    // Show selected section
    const section = document.getElementById(sectionId);
    if (section) {
        section.style.display = 'block';
        if (!silent) {
            motionBlur(section, { dir: 'x', distance: -70, duration: 480, maxBlur: 12 });
        }
    }

    // Update nav items
    document.querySelectorAll('.nav-item').forEach(item => {
        item.classList.toggle('active', item.dataset.section === sectionId);
    });

    if (sectionId === 'reports') updateReports();
    if (sectionId === 'points') updatePointsView();
    if (sectionId === 'speeches') updateSpeechView();
    if (sectionId === 'marksheet') updateMarksheet();
    if (!silent) animateRows(sectionId);
}

// Table rows blur in one after another
function animateRows(sectionId) {
    const rows = document.querySelectorAll('#' + sectionId + ' .data-table tbody tr');
    rows.forEach((row, i) => {
        row.classList.remove('mb-row');
        void row.offsetWidth; // restart animation
        row.style.setProperty('--i', Math.min(i, 12));
        row.classList.add('mb-row');
    });
}

// Open / close modals with a vertical motion blur
function openModal(id) {
    const modal = document.getElementById(id);
    modal.style.display = 'flex';
    motionBlur(modal.querySelector('.modal-content'), { dir: 'y', distance: 60, duration: 380, maxBlur: 9 });
}

// Update dashboard stats
function updateDashboard() {
    document.getElementById('totalDelegates').textContent = appData.delegates.length;
    document.getElementById('totalCommittees').textContent = appData.committees.length;
    document.getElementById('totalSessions').textContent = appData.sessions.length;
    document.getElementById('totalPendingPoints').textContent =
        points.filter(p => p.status === 'Pending').length;
}

// ===== DELEGATES =====
function delegateRow(delegate) {
    return `
        <tr>
            <td>${esc(delegate.id)}</td>
            <td>${esc(delegate.name)}</td>
            <td>${esc(delegate.country)}</td>
            <td>${esc(delegate.committee)}</td>
            <td>${esc(delegate.score)}</td>
            <td class="edit-only">
                <button onclick="editDelegate('${delegate.id}')" class="btn-small">Edit</button>
                <button onclick="deleteDelegate('${delegate.id}')" class="btn-small btn-danger">Delete</button>
            </td>
        </tr>`;
}

// Update delegates table (respects the search box)
function updateDelegatesTable() {
    const tbody = document.getElementById('delegatesTableBody');
    const searchTerm = document.getElementById('searchDelegates').value.toLowerCase();

    if (appData.delegates.length === 0) {
        tbody.innerHTML = '<tr><td colspan="6" class="text-center">No delegates yet</td></tr>';
        return;
    }

    const filtered = appData.delegates.filter(d =>
        d.name.toLowerCase().includes(searchTerm) ||
        d.country.toLowerCase().includes(searchTerm) ||
        d.committee.toLowerCase().includes(searchTerm)
    );

    if (filtered.length === 0) {
        tbody.innerHTML = '<tr><td colspan="6" class="text-center">No delegates found</td></tr>';
        return;
    }

    tbody.innerHTML = filtered.map(delegateRow).join('');
}

function searchDelegates() {
    updateDelegatesTable();
}

function showAddDelegateForm() {
    if (!requireEdit()) return;
    openModal('delegateModal');
}

function closeDelegateModal() {
    document.getElementById('delegateModal').style.display = 'none';
}

function addDelegate(event) {
    event.preventDefault();
    if (!requireEdit()) return;

    const name = document.getElementById('delName').value;
    const country = document.getElementById('delCountry').value;
    const committee = document.getElementById('delCommittee').value;

    if (name && country && committee) {
        appData.delegates.push({
            id: nextId('D', appData.delegates, 3),
            name: name,
            country: country,
            committee: committee,
            score: 0
        });

        saveData();
        updateDelegatesTable();
        updateDashboard();
        closeDelegateModal();

        document.getElementById('delName').value = '';
        document.getElementById('delCountry').value = '';
        document.getElementById('delCommittee').value = '';

        alert('Delegate added successfully!');
    }
}

function editDelegate(id) {
    if (!requireEdit()) return;
    const delegate = appData.delegates.find(d => d.id === id);
    if (delegate) {
        const newName = prompt('Enter new name:', delegate.name);
        if (newName) {
            delegate.name = newName;
            saveData();
            updateDelegatesTable();
            alert('Delegate updated successfully!');
        }
    }
}

function deleteDelegate(id) {
    if (!requireEdit()) return;
    if (confirm('Are you sure you want to delete this delegate?')) {
        appData.delegates = appData.delegates.filter(d => d.id !== id);
        saveData();
        updateDelegatesTable();
        updateDashboard();
        alert('Delegate deleted successfully!');
    }
}

// ===== COMMITTEES =====
function updateCommitteesTable() {
    const tbody = document.getElementById('committeesTableBody');

    if (appData.committees.length === 0) {
        tbody.innerHTML = '<tr><td colspan="5" class="text-center">No committees yet</td></tr>';
        return;
    }

    tbody.innerHTML = appData.committees.map(committee => `
        <tr>
            <td>${esc(committee.id)}</td>
            <td>${esc(committee.name)}</td>
            <td>${esc(committee.capacity)}</td>
            <td>${esc(committee.members)}</td>
            <td class="edit-only">
                <button onclick="editCommittee('${committee.id}')" class="btn-small">Edit</button>
                <button onclick="deleteCommittee('${committee.id}')" class="btn-small btn-danger">Delete</button>
            </td>
        </tr>
    `).join('');
}

function showAddCommitteeForm() {
    if (!requireEdit()) return;
    openModal('committeeModal');
}

function closeCommitteeModal() {
    document.getElementById('committeeModal').style.display = 'none';
}

function addCommittee(event) {
    event.preventDefault();
    if (!requireEdit()) return;

    const name = document.getElementById('comName').value;
    const capacity = parseInt(document.getElementById('comCapacity').value);

    if (name && capacity > 0) {
        appData.committees.push({
            id: nextId('COM', appData.committees, 3),
            name: name,
            capacity: capacity,
            members: 0
        });

        saveData();
        updateCommitteesTable();
        updateDashboard();
        closeCommitteeModal();

        document.getElementById('comName').value = '';
        document.getElementById('comCapacity').value = '';

        alert('Committee added successfully!');
    }
}

function editCommittee(id) {
    if (!requireEdit()) return;
    const committee = appData.committees.find(c => c.id === id);
    if (committee) {
        const newName = prompt('Enter new name:', committee.name);
        if (newName) {
            committee.name = newName;
            saveData();
            updateCommitteesTable();
            alert('Committee updated successfully!');
        }
    }
}

function deleteCommittee(id) {
    if (!requireEdit()) return;
    if (confirm('Are you sure you want to delete this committee?')) {
        appData.committees = appData.committees.filter(c => c.id !== id);
        saveData();
        updateCommitteesTable();
        updateDashboard();
        alert('Committee deleted successfully!');
    }
}

// ===== SESSIONS =====
function updateSessionsTable() {
    const tbody = document.getElementById('sessionsTableBody');

    if (appData.sessions.length === 0) {
        tbody.innerHTML = '<tr><td colspan="5" class="text-center">No sessions yet</td></tr>';
        return;
    }

    tbody.innerHTML = appData.sessions.map(session => `
        <tr>
            <td>${esc(session.id)}</td>
            <td>${esc(session.name)}</td>
            <td>${esc(session.date)}</td>
            <td>${esc(session.status)}</td>
            <td class="edit-only">
                <button onclick="editSession('${session.id}')" class="btn-small">Edit</button>
                <button onclick="deleteSession('${session.id}')" class="btn-small btn-danger">Delete</button>
            </td>
        </tr>
    `).join('');
}

function showAddSessionForm() {
    if (!requireEdit()) return;
    openModal('sessionModal');
}

function closeSessionModal() {
    document.getElementById('sessionModal').style.display = 'none';
}

function addSession(event) {
    event.preventDefault();
    if (!requireEdit()) return;

    const name = document.getElementById('sesName').value;
    const date = document.getElementById('sesDate').value;

    if (name && date) {
        appData.sessions.push({
            id: nextId('SESS', appData.sessions, 3),
            name: name,
            date: date,
            status: 'Planning'
        });

        saveData();
        updateSessionsTable();
        updateDashboard();
        closeSessionModal();

        document.getElementById('sesName').value = '';
        document.getElementById('sesDate').value = '';

        alert('Session added successfully!');
    }
}

function editSession(id) {
    if (!requireEdit()) return;
    const session = appData.sessions.find(s => s.id === id);
    if (session) {
        const newName = prompt('Enter new name:', session.name);
        if (newName) {
            session.name = newName;
            saveData();
            updateSessionsTable();
            alert('Session updated successfully!');
        }
    }
}

function deleteSession(id) {
    if (!requireEdit()) return;
    if (confirm('Are you sure you want to delete this session?')) {
        appData.sessions = appData.sessions.filter(s => s.id !== id);
        saveData();
        updateSessionsTable();
        updateDashboard();
        alert('Session deleted successfully!');
    }
}

// ===== POINTS OF INFORMATION (POI) / POINTS OF ORDER (POO) =====
const POINT_INFO = {
    POI: {
        name: 'Point of Information',
        hint: 'A question directed to the speaker or to the Chair, e.g. a factual or clarifying query.',
        rulings: [['Accepted', 'Accept'], ['Rejected', 'Reject']]
    },
    POO: {
        name: 'Point of Order',
        hint: 'Raised when the rules of procedure appear to have been breached. The Chair rules immediately.',
        rulings: [['Sustained', 'Sustain'], ['Overruled', 'Overrule']]
    }
};

function loadPoints() {
    const saved = localStorage.getItem('munsphere-points');
    if (saved) {
        points = JSON.parse(saved);
    } else {
        // Small sample so the feature is visible on first run
        const now = Date.now();
        points = [
            { id: 'P001', type: 'POO', committee: 'UNGA', by: 'Aarav Sharma', role: 'delegate',
              subject: 'Speaker exceeded the allotted speaking time', status: 'Pending',
              time: now - 4 * 60000, ruledBy: '', ruledAt: null },
            { id: 'P002', type: 'POI', committee: 'UNHRC', by: 'Emma Wilson', role: 'delegate',
              subject: 'Does the delegate of France support the amendment in clause 3?', status: 'Pending',
              time: now - 2 * 60000, ruledBy: '', ruledAt: null },
            { id: 'P003', type: 'POI', committee: 'WHO', by: 'Wei Chen', role: 'delegate',
              subject: 'Request for clarification on the funding source', status: 'Accepted',
              time: now - 30 * 60000, ruledBy: 'Executive Board', ruledAt: now - 28 * 60000 }
        ];
        savePoints();
    }
    updatePointsView();
}

function savePoints() {
    localStorage.setItem('munsphere-points', JSON.stringify(points));
}

function fmtTime(ts) {
    return new Date(ts).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
}

function refreshCommitteeOptions() {
    const names = appData.committees.map(c => c.name);

    const filter = document.getElementById('pointCommitteeFilter');
    const keep = filter.value;
    filter.innerHTML = '<option value="">All committees</option>' +
        names.map(n => `<option value="${esc(n)}">${esc(n)}</option>`).join('');
    filter.value = names.includes(keep) ? keep : '';

    document.getElementById('pointCommittee').innerHTML =
        names.map(n => `<option value="${esc(n)}">${esc(n)}</option>`).join('');
}

function updatePointsView() {
    document.getElementById('pointsNote').style.display = isOC() ? 'none' : 'block';
    refreshCommitteeOptions();

    const filterValue = document.getElementById('pointCommitteeFilter').value;
    const inScope = p => !filterValue || p.committee === filterValue;

    const pending = points.filter(p => p.status === 'Pending');
    document.getElementById('pendingPOI').textContent = pending.filter(p => p.type === 'POI').length;
    document.getElementById('pendingPOO').textContent = pending.filter(p => p.type === 'POO').length;
    document.getElementById('ruledPoints').textContent = points.filter(p =>
        p.status !== 'Pending' && p.status !== 'Withdrawn' &&
        new Date(p.ruledAt).toDateString() === new Date().toDateString()).length;

    // Pending nav badge + dashboard card
    const badge = document.getElementById('pendingBadge');
    badge.textContent = pending.length;
    badge.style.display = pending.length ? 'inline-block' : 'none';
    updateDashboard();

    // Queue: POO first (privileged), then by time raised
    const queue = pending.filter(inScope).sort((a, b) =>
        (a.type === b.type ? 0 : a.type === 'POO' ? -1 : 1) || a.time - b.time);

    const queueEl = document.getElementById('pointQueue');
    queueEl.innerHTML = queue.length === 0
        ? '<p class="empty-queue">No pending points. The floor is clear.</p>'
        : queue.map(pointCard).join('');

    // History
    const history = points.filter(p => p.status !== 'Pending').filter(inScope)
        .sort((a, b) => (b.ruledAt || b.time) - (a.ruledAt || a.time));
    const tbody = document.getElementById('pointHistoryBody');
    tbody.innerHTML = history.length === 0
        ? '<tr><td colspan="6" class="text-center">No points yet</td></tr>'
        : history.map(p => `
            <tr>
                <td><span class="point-type ${p.type}">${p.type}</span></td>
                <td>${esc(p.committee)}</td>
                <td>${esc(p.by)}</td>
                <td>${esc(p.subject)}</td>
                <td><span class="outcome outcome-${p.status}">${esc(p.status)}</span></td>
                <td>${esc(p.ruledBy || '-')}</td>
            </tr>`).join('');
}

function pointCard(p) {
    const info = POINT_INFO[p.type];
    let actions = '';

    if (isOC()) {
        actions += info.rulings.map(([status, label], i) =>
            `<button onclick="rulePoint('${p.id}','${status}')" class="btn-small ${i === 1 ? 'btn-danger' : ''}">${label}</button>`
        ).join('');
        actions += `<button onclick="deletePoint('${p.id}')" class="btn-small btn-ghost">Remove</button>`;
    } else if (p.by === currentUsername) {
        actions = `<button onclick="withdrawPoint('${p.id}')" class="btn-small btn-ghost">Withdraw</button>`;
    }

    return `
        <div class="point-card ${p.type}">
            <div class="point-main">
                <div class="point-meta">
                    <span class="point-type ${p.type}">${p.type}</span>
                    <strong>${esc(info.name)}</strong>
                    <span class="point-where">${esc(p.committee)} &middot; ${esc(p.by)} &middot; ${fmtTime(p.time)}</span>
                </div>
                <p class="point-subject">${esc(p.subject)}</p>
            </div>
            <div class="point-card-actions">${actions}</div>
        </div>`;
}

function openPointModal(type) {
    if (appData.committees.length === 0) {
        alert('There are no committees yet. Ask the Executive Board or Organising Committee to add one.');
        return;
    }
    refreshCommitteeOptions();
    document.getElementById('pointType').value = type;
    document.getElementById('pointModalTitle').textContent = 'Raise a ' + POINT_INFO[type].name;
    document.getElementById('pointModalHint').textContent = POINT_INFO[type].hint;
    openModal('pointModal');
}

function closePointModal() {
    document.getElementById('pointModal').style.display = 'none';
}

function submitPoint(event) {
    event.preventDefault();

    const type = document.getElementById('pointType').value;
    const committee = document.getElementById('pointCommittee').value;
    const subject = document.getElementById('pointSubject').value.trim();
    if (!POINT_INFO[type] || !committee || !subject) return;

    points.push({
        id: nextId('P', points, 3),
        type: type,
        committee: committee,
        by: currentUsername,
        role: currentRole,
        subject: subject,
        status: 'Pending',
        time: Date.now(),
        ruledBy: '',
        ruledAt: null
    });

    savePoints();
    document.getElementById('pointSubject').value = '';
    closePointModal();
    updatePointsView();
}

// Organising Committee only
function rulePoint(id, status) {
    if (!requireOC()) return;
    const p = points.find(x => x.id === id);
    if (!p || p.status !== 'Pending') return;
    p.status = status;
    p.ruledBy = ROLES[currentRole].label;
    p.ruledAt = Date.now();
    savePoints();
    updatePointsView();
}

function deletePoint(id) {
    if (!requireOC()) return;
    if (confirm('Remove this point from the queue?')) {
        points = points.filter(p => p.id !== id);
        savePoints();
        updatePointsView();
    }
}

function clearRuledPoints() {
    if (!requireOC()) return;
    if (confirm('Clear all ruled points from the history?')) {
        points = points.filter(p => p.status === 'Pending');
        savePoints();
        updatePointsView();
    }
}

// A delegate can withdraw only their own pending point
function withdrawPoint(id) {
    const p = points.find(x => x.id === id);
    if (!p || p.status !== 'Pending' || p.by !== currentUsername) return;
    p.status = 'Withdrawn';
    p.ruledBy = currentUsername;
    p.ruledAt = Date.now();
    savePoints();
    updatePointsView();
}

// ===== SPEECH MARKING (EXECUTIVE BOARD ONLY) =====
const MAX_SPEECH_FILE = 1.5 * 1024 * 1024; // browser storage is limited (~5 MB total)

let speechData = { sessions: [], speeches: [] };
let activeSpeechSession = '';

function loadSpeeches() {
    const saved = localStorage.getItem('munsphere-speeches');
    if (saved) {
        speechData = JSON.parse(saved);
    } else {
        speechData = {
            sessions: [
                { id: 'SP001', name: 'Private Session', max: 10 },
                { id: 'SP002', name: 'Public JSL',      max: 10 },
                { id: 'SP003', name: 'Solution',        max: 10 }
            ],
            speeches: []
        };
        saveSpeeches();
    }
    activeSpeechSession = speechData.sessions.length ? speechData.sessions[0].id : '';
}

// Returns false if the browser storage quota is exceeded
function saveSpeeches() {
    try {
        localStorage.setItem('munsphere-speeches', JSON.stringify(speechData));
        return true;
    } catch (e) {
        alert('Could not save: browser storage is full. Delete some uploaded speeches or use smaller files.');
        return false;
    }
}

function getSpeechSession(id) {
    return speechData.sessions.find(x => x.id === id);
}

function updateSpeechView() {
    if (!isEB()) return;

    // Subsection tabs
    const tabs = document.getElementById('speechTabs');
    tabs.innerHTML = speechData.sessions.map(sess => {
        const count = speechData.speeches.filter(sp => sp.sessionId === sess.id).length;
        return `<button class="speech-tab ${sess.id === activeSpeechSession ? 'active' : ''}"
                    onclick="selectSpeechSession('${sess.id}')">${esc(sess.name)}<span class="tab-count">(${count})</span></button>`;
    }).join('');

    const sess = getSpeechSession(activeSpeechSession);
    const info = document.getElementById('speechSessionInfo');
    const tbody = document.getElementById('speechTableBody');

    if (!sess) {
        info.textContent = '';
        tbody.innerHTML = '<tr><td colspan="7" class="text-center">No sessions yet. Add one to begin.</td></tr>';
    } else {
        info.textContent = sess.name + ': marks are out of ' + sess.max + '.';
        const rows = speechData.speeches.filter(sp => sp.sessionId === sess.id);

        tbody.innerHTML = rows.length === 0
            ? '<tr><td colspan="7" class="text-center">No speeches uploaded for this session yet</td></tr>'
            : rows.map(sp => `
                <tr>
                    <td>${esc(sp.delegateName)}</td>
                    <td>${esc(sp.country)}</td>
                    <td>${esc(sp.title)}</td>
                    <td><button class="btn-small btn-ghost" onclick="downloadSpeech('${sp.id}')">${esc(sp.fileName)}</button></td>
                    <td>
                        <input type="number" class="marks-input" id="marks-${sp.id}" min="0" max="${sess.max}" step="0.5"
                               value="${sp.marks === null ? '' : sp.marks}" placeholder="-">
                        <span class="marks-max">/ ${sess.max}</span>
                    </td>
                    <td><input type="text" class="remarks-input" id="remarks-${sp.id}" maxlength="200"
                               value="${esc(sp.remarks || '')}" placeholder="Optional remarks"></td>
                    <td>
                        <button class="btn-small" onclick="saveMarks('${sp.id}')">Save</button>
                        <button class="btn-small btn-danger" onclick="deleteSpeech('${sp.id}')">Delete</button>
                        ${sp.marks !== null ? '<span class="marked-ok">Marked</span>' : ''}
                    </td>
                </tr>`).join('');
    }

    updateSpeechTotals();
}

function selectSpeechSession(id) {
    activeSpeechSession = id;
    updateSpeechView();
    motionBlur(document.getElementById('speechTableBody'), { dir: 'x', distance: -50, duration: 380, maxBlur: 9 });
}

function updateSpeechTotals() {
    const byDelegate = {};
    speechData.speeches.forEach(sp => {
        if (sp.marks === null) return;
        const sess = getSpeechSession(sp.sessionId);
        const key = sp.delegateId + '|' + sp.delegateName;
        if (!byDelegate[key]) {
            byDelegate[key] = { name: sp.delegateName, country: sp.country, count: 0, total: 0, max: 0 };
        }
        byDelegate[key].count++;
        byDelegate[key].total += sp.marks;
        byDelegate[key].max += sess ? sess.max : 0;
    });

    const list = Object.values(byDelegate).sort((a, b) => b.total - a.total);
    document.getElementById('speechTotalsBody').innerHTML = list.length === 0
        ? '<tr><td colspan="5" class="text-center">No marks yet</td></tr>'
        : list.map((d, i) => `
            <tr>
                <td>${i + 1}</td>
                <td>${esc(d.name)}</td>
                <td>${esc(d.country)}</td>
                <td>${d.count}</td>
                <td>${d.total} / ${d.max}</td>
            </tr>`).join('');
}

// --- Upload a speech ---
function openSpeechModal() {
    if (!requireEB()) return;
    const sess = getSpeechSession(activeSpeechSession);
    if (!sess) { alert('Add a session first.'); return; }
    if (appData.delegates.length === 0) { alert('There are no delegates to attach a speech to.'); return; }

    document.getElementById('speechModalSession').textContent = 'Session: ' + sess.name;
    document.getElementById('speechDelegate').innerHTML = appData.delegates.map(d =>
        `<option value="${esc(d.id)}">${esc(d.name)} (${esc(d.country)}, ${esc(d.committee)})</option>`).join('');
    openModal('speechModal');
}

function closeSpeechModal() {
    document.getElementById('speechModal').style.display = 'none';
}

function submitSpeech(event) {
    event.preventDefault();
    if (!requireEB()) return;

    const delegate = appData.delegates.find(d => d.id === document.getElementById('speechDelegate').value);
    const title = document.getElementById('speechTitle').value.trim();
    const file = document.getElementById('speechFile').files[0];
    if (!delegate || !title || !file) return;

    if (file.size > MAX_SPEECH_FILE) {
        alert('File is too large (' + (file.size / 1024 / 1024).toFixed(1) + ' MB). Maximum is 1.5 MB.');
        return;
    }

    const reader = new FileReader();
    reader.onload = function () {
        speechData.speeches.push({
            id: nextId('SPC', speechData.speeches, 3),
            sessionId: activeSpeechSession,
            delegateId: delegate.id,
            delegateName: delegate.name,
            country: delegate.country,
            title: title,
            fileName: file.name,
            fileData: reader.result,
            marks: null,
            remarks: '',
            markedBy: '',
            time: Date.now()
        });

        if (!saveSpeeches()) {
            speechData.speeches.pop(); // roll back if storage was full
            return;
        }

        document.getElementById('speechTitle').value = '';
        document.getElementById('speechFile').value = '';
        closeSpeechModal();
        updateSpeechView();
    };
    reader.onerror = function () { alert('Could not read the file.'); };
    reader.readAsDataURL(file);
}

function downloadSpeech(id) {
    if (!requireEB()) return;
    const sp = speechData.speeches.find(x => x.id === id);
    if (!sp) return;
    const a = document.createElement('a');
    a.href = sp.fileData;
    a.download = sp.fileName;
    document.body.appendChild(a);
    a.click();
    a.remove();
}

// --- Marking ---
function saveMarks(id) {
    if (!requireEB()) return;
    const sp = speechData.speeches.find(x => x.id === id);
    const sess = sp && getSpeechSession(sp.sessionId);
    if (!sp || !sess) return;

    const raw = document.getElementById('marks-' + id).value;
    const marks = parseFloat(raw);

    if (raw === '' || isNaN(marks) || marks < 0 || marks > sess.max) {
        alert('Enter marks between 0 and ' + sess.max + '.');
        return;
    }

    sp.marks = marks;
    sp.remarks = document.getElementById('remarks-' + id).value.trim();
    sp.markedBy = currentUsername;
    if (saveSpeeches()) updateSpeechView();
}

function deleteSpeech(id) {
    if (!requireEB()) return;
    if (confirm('Delete this speech and its marks?')) {
        speechData.speeches = speechData.speeches.filter(x => x.id !== id);
        saveSpeeches();
        updateSpeechView();
    }
}

// --- Add more sessions ---
function openSpeechSessionModal() {
    if (!requireEB()) return;
    openModal('speechSessionModal');
}

function closeSpeechSessionModal() {
    document.getElementById('speechSessionModal').style.display = 'none';
}

function submitSpeechSession(event) {
    event.preventDefault();
    if (!requireEB()) return;

    const name = document.getElementById('speechSessionName').value.trim();
    const max = parseInt(document.getElementById('speechSessionMax').value, 10);
    if (!name || !(max > 0)) return;

    if (speechData.sessions.some(x => x.name.toLowerCase() === name.toLowerCase())) {
        alert('A session with that name already exists.');
        return;
    }

    const sess = { id: nextId('SP', speechData.sessions, 3), name: name, max: max };
    speechData.sessions.push(sess);
    saveSpeeches();
    activeSpeechSession = sess.id;

    document.getElementById('speechSessionName').value = '';
    document.getElementById('speechSessionMax').value = 10;
    closeSpeechSessionModal();
    updateSpeechView();
}

// ===== MAIN MARKSHEET (view: everyone, edit: Organising Committee only) =====
function updateMarksheet() {
    document.getElementById('marksheetNote').style.display = isOC() ? 'none' : 'block';

    const sessions = speechData.sessions;
    const head = document.getElementById('marksheetHead');
    head.innerHTML = '<tr><th>ID</th><th>Delegate</th><th>Country</th><th>Committee</th>' +
        sessions.map(x => `<th>${esc(x.name)} <span class="marks-max">/ ${x.max}</span></th>`).join('') +
        '<th>Speech Total</th><th>Main Score <span class="marks-max">/ 100</span></th>' +
        '<th class="oc-only">Actions</th></tr>';

    const tbody = document.getElementById('marksheetBody');
    if (appData.delegates.length === 0) {
        tbody.innerHTML = `<tr><td colspan="${sessions.length + 7}" class="text-center">No delegates yet</td></tr>`;
        return;
    }

    tbody.innerHTML = appData.delegates.map(d => {
        let total = 0, maxTotal = 0, any = false;
        const cells = sessions.map(sess => {
            const marks = speechData.speeches
                .filter(sp => sp.delegateId === d.id && sp.sessionId === sess.id && sp.marks !== null)
                .map(sp => sp.marks);
            if (marks.length === 0) return '<td>-</td>';
            const best = Math.max(...marks);
            total += best; maxTotal += sess.max; any = true;
            return `<td>${best}</td>`;
        }).join('');

        const scoreCell = isOC()
            ? `<td><input type="number" class="marks-input" id="score-${d.id}" min="0" max="100" step="0.5" value="${d.score}"></td>`
            : `<td><strong>${esc(d.score)}</strong></td>`;

        return `
            <tr>
                <td>${esc(d.id)}</td>
                <td>${esc(d.name)}</td>
                <td>${esc(d.country)}</td>
                <td>${esc(d.committee)}</td>
                ${cells}
                <td>${any ? total + ' / ' + maxTotal : '-'}</td>
                ${scoreCell}
                <td class="oc-only"><button class="btn-small" onclick="saveMainScore('${d.id}')">Save</button></td>
            </tr>`;
    }).join('');
}

function saveMainScore(id) {
    if (!requireOC()) return;
    const d = appData.delegates.find(x => x.id === id);
    if (!d) return;

    const raw = document.getElementById('score-' + id).value;
    const score = parseFloat(raw);
    if (raw === '' || isNaN(score) || score < 0 || score > 100) {
        alert('Enter a score between 0 and 100.');
        return;
    }

    d.score = score;
    saveData();
    updateMarksheet();
    updateDelegatesTable();
}

// ===== REPORTS =====
function updateReports() {
    document.getElementById('reportTotalDelegates').textContent = appData.delegates.length;
    document.getElementById('reportTotalCommittees').textContent = appData.committees.length;
    document.getElementById('reportTotalSessions').textContent = appData.sessions.length;
    document.getElementById('reportTotalPOI').textContent = points.filter(p => p.type === 'POI').length;
    document.getElementById('reportTotalPOO').textContent = points.filter(p => p.type === 'POO').length;

    // Display top scoring delegates
    const sorted = [...appData.delegates].sort((a, b) => b.score - a.score);
    const tbody = document.getElementById('topDelegatesBody');

    tbody.innerHTML = sorted.slice(0, 5).map((delegate, index) => `
        <tr>
            <td>${index + 1}</td>
            <td>${esc(delegate.name)}</td>
            <td>${esc(delegate.country)}</td>
            <td>${esc(delegate.score)}</td>
        </tr>
    `).join('');
}

// Logout function
function logout() {
    if (confirm('Are you sure you want to logout?')) {
        localStorage.removeItem('loggedIn');
        localStorage.removeItem('username');
        localStorage.removeItem('role');
        window.location.href = 'index.html';
    }
}

// Close modal when clicking outside
window.onclick = function (event) {
    document.querySelectorAll('.modal').forEach(modal => {
        if (event.target === modal) {
            modal.style.display = 'none';
        }
    });
};
