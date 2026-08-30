/* Promise-based wrapper around the WebView2 message channel.
 *
 *   const user = await bridge.call('auth:login', { username, password });
 *   bridge.on('feed:updated', data => { ... });
 *
 * When the page is opened in a plain browser (no WebView2 host) the calls are
 * answered by the mock at the bottom of this file, so the UI can be developed
 * without building the .exe.
 */
(function (global) {
    'use strict';

    var host = global.chrome && global.chrome.webview ? global.chrome.webview : null;
    var pending = new Map();
    var listeners = new Map();
    var nextId = 0;

    function settle(message) {
        // Reply to a call().
        if (message.id !== undefined && pending.has(message.id)) {
            var entry = pending.get(message.id);
            pending.delete(message.id);
            if (message.ok) {
                entry.resolve(message.data);
            } else {
                entry.reject(new Error(message.error || 'Unbekannter Fehler'));
            }
            return;
        }
        // Unsolicited push from C++.
        if (message.channel && listeners.has(message.channel)) {
            listeners.get(message.channel).forEach(function (fn) {
                fn(message.data);
            });
        }
    }

    if (host) {
        host.addEventListener('message', function (event) {
            var data = event.data;
            if (typeof data === 'string') {
                try {
                    data = JSON.parse(data);
                } catch (error) {
                    return;
                }
            }
            if (data && typeof data === 'object') {
                settle(data);
            }
        });
    }

    var bridge = {
        /** True when running inside the C++ host. */
        native: Boolean(host),

        /** Calls a backend channel and resolves with its data. */
        call: function (channel, payload) {
            if (!host) {
                return bridge._mock(channel, payload || {});
            }
            var id = String(++nextId);
            return new Promise(function (resolve, reject) {
                pending.set(id, { resolve: resolve, reject: reject });
                host.postMessage({ id: id, channel: channel, payload: payload || {} });
            });
        },

        /** Subscribes to a push channel. Returns an unsubscribe function. */
        on: function (channel, handler) {
            if (!listeners.has(channel)) {
                listeners.set(channel, []);
            }
            listeners.get(channel).push(handler);
            return function () {
                var all = listeners.get(channel) || [];
                var index = all.indexOf(handler);
                if (index >= 0) all.splice(index, 1);
            };
        },

        /**
         * Fire-and-forget window control.
         * @param action 'close' | 'minimize' | 'drag' | 'resize'
         * @param size   {width, height} in CSS pixels, for 'resize'
         */
        window: function (action, size) {
            if (host) {
                host.postMessage({
                    channel: 'window:command',
                    payload: {
                        action: action,
                        width: size ? Math.round(size.width) : 0,
                        height: size ? Math.round(size.height) : 0
                    }
                });
            } else if (action === 'close') {
                global.close();
            }
        },

        /* --- browser-only fallback ------------------------------------ */
        _mock: function (channel, payload) {
            switch (channel) {
                case 'auth:login':
                    if (payload.username === 'demo' && payload.password === 'demo') {
                        return Promise.resolve({
                            username: 'demo',
                            displayName: 'CYKA BLYAT',
                            balance: 12500
                        });
                    }
                    return Promise.reject(new Error('Username oder Passwort ist falsch.'));
                case 'auth:rememberedUser':
                    return Promise.resolve({ username: '' });
                case 'auth:logout':
                    return Promise.resolve({});
                case 'feed:adverts':
                    return Promise.resolve({ items: [] });
                case 'feed:quest':
                    return Promise.resolve({
                        index: 2,
                        title: 'Der Angler',
                        description: 'Du musst 10 Fische fangen',
                        progress: 5,
                        goal: 10
                    });
                default:
                    return Promise.reject(new Error('Unknown channel: ' + channel));
            }
        }
    };

    global.bridge = bridge;
})(window);
