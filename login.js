// MUNSphere Login Script

const VALID_ROLES = ['delegate', 'eb', 'oc'];

function handleLogin(event) {
    event.preventDefault();

    const username = document.getElementById('username').value.trim();
    const password = document.getElementById('password').value;
    const role = document.querySelector('input[name="role"]:checked').value;
    const errorMessage = document.getElementById('errorMessage');

    // Check password - must be exactly 123@geu
    if (password === '123@geu' && VALID_ROLES.includes(role)) {
        // Login successful
        localStorage.setItem('username', username);
        localStorage.setItem('role', role);
        localStorage.setItem('loggedIn', 'true');

        // Slide the card away with motion blur, then go to the dashboard
        motionBlur(document.getElementById('loginBox'), {
            dir: 'x', distance: 220, duration: 380, maxBlur: 14, mode: 'out'
        }).then(() => {
            window.location.href = 'dashboard.html';
        });
    } else {
        // Login failed
        errorMessage.textContent = 'Invalid password. Please try again.';
        errorMessage.style.display = 'block';

        // Clear password field
        document.getElementById('password').value = '';
    }
}

window.addEventListener('load', function () {
    // Already logged in with a valid role -> straight to dashboard
    if (localStorage.getItem('loggedIn') === 'true' &&
        VALID_ROLES.includes(localStorage.getItem('role'))) {
        window.location.href = 'dashboard.html';
        return;
    }

    // Entrance: card slides in from the left with motion blur
    motionBlur(document.getElementById('loginBox'), {
        dir: 'x', distance: -160, duration: 650, maxBlur: 16
    });
});
