/* View routing, window chrome and the login flow. */
(function () {
    'use strict';

    /* --- window sizing --------------------------------------------------- */

    // The host window is frameless and has no size of its own: it is resized to
    // whatever panel is on screen, so the app appears as just that panel.
    function fitWindowToPanel() {
        var panel = document.querySelector('.view.is-active .panel');
        if (!panel) return;
        bridge.window('resize', {
            width: panel.offsetWidth,
            height: panel.offsetHeight
        });
    }

    /* --- routing -------------------------------------------------------- */

    function showView(name) {
        document.querySelectorAll('.view').forEach(function (view) {
            view.classList.toggle('is-active', view.dataset.view === name);
        });
        fitWindowToPanel();
    }

    /* --- window chrome -------------------------------------------------- */

    document.addEventListener('click', function (event) {
        var trigger = event.target.closest('[data-window-action]');
        if (trigger) {
            bridge.window(trigger.dataset.windowAction);
        }
    });

    document.addEventListener('mousedown', function (event) {
        if (event.button !== 0) return;
        if (event.target.closest('[data-drag-window]') && !event.target.closest('button, input, label, a')) {
            bridge.window('drag');
        }
    });

    document.addEventListener('keydown', function (event) {
        if (event.key === 'Escape') {
            bridge.window('close');
        }
    });

    /* --- login ---------------------------------------------------------- */

    var form = document.getElementById('loginForm');
    var submit = document.getElementById('loginSubmit');
    var errorLine = document.getElementById('loginError');
    var usernameInput = document.getElementById('username');
    var passwordInput = document.getElementById('password');
    var rememberInput = document.getElementById('remember');

    function setError(message) {
        errorLine.textContent = message || '';
    }

    form.addEventListener('submit', function (event) {
        event.preventDefault();
        setError('');
        submit.disabled = true;

        bridge
            .call('auth:login', {
                username: usernameInput.value,
                password: passwordInput.value,
                remember: rememberInput.checked
            })
            .then(function (user) {
                passwordInput.value = '';
                document.dispatchEvent(new CustomEvent('auth:signedin', { detail: user }));
                showView('dashboard');
            })
            .catch(function (error) {
                setError(error.message);
                passwordInput.value = '';
                passwordInput.focus();
            })
            .finally(function () {
                submit.disabled = false;
            });
    });

    // Clear a stale error as soon as the user starts correcting the input.
    [usernameInput, passwordInput].forEach(function (input) {
        input.addEventListener('input', function () {
            if (errorLine.textContent) setError('');
        });
    });

    /* --- boot ----------------------------------------------------------- */

    bridge
        .call('auth:rememberedUser')
        .then(function (result) {
            if (result && result.username) {
                usernameInput.value = result.username;
                rememberInput.checked = true;
                passwordInput.focus();
            } else {
                usernameInput.focus();
            }
        })
        .catch(function () {
            usernameInput.focus();
        });

    fitWindowToPanel();
})();
