/* SPDX-License-Identifier: GPL-2.0-or-later */
(() => {
    'use strict';

    const initialise = () => {
        const picker = document.getElementById('theme-picker');
        const preference = window.matchMedia('(prefers-color-scheme: dark)');

        if (picker instanceof HTMLSelectElement) {
            const applyTheme = () => {
                const dark = picker.value === 'dark'
                    || (picker.value === 'system' && preference.matches);
                document.documentElement.dataset.theme = dark ? 'dark' : 'light';
                document.querySelectorAll('meta[name="theme-color"]').forEach((meta) => {
                    meta.setAttribute('content', dark ? '#111416' : '#ffffff');
                });
            };

            // Follow the OS on every visit, including browser back/forward navigation.
            const resetTheme = () => {
                picker.value = 'system';
                applyTheme();
            };
            picker.addEventListener('change', applyTheme);
            preference.addEventListener('change', applyTheme);
            window.addEventListener('pageshow', resetTheme);
            resetTheme();
        }

        const navigation = document.getElementById('docs-navigation');
        const compact = window.matchMedia('(max-width: 760px)');
        if (navigation instanceof HTMLDetailsElement) {
            const fitNavigation = () => { navigation.open = !compact.matches; };
            compact.addEventListener('change', fitNavigation);
            fitNavigation();
        }
    };

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initialise, { once: true });
    } else {
        initialise();
    }
})();
