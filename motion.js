// MUNSphere Motion Blur helper
// Directional motion blur: the element slides while an SVG Gaussian blur
// (blurred only along the axis of travel) is scaled to the element's velocity,
// so it is blurriest when moving fastest and sharp when it settles.

(function () {
    const NS = 'http://www.w3.org/2000/svg';
    let counter = 0;
    let host = null;

    const reduced = window.matchMedia &&
        window.matchMedia('(prefers-reduced-motion: reduce)').matches;

    function getHost() {
        if (host) return host;
        host = document.createElementNS(NS, 'svg');
        host.setAttribute('width', '0');
        host.setAttribute('height', '0');
        host.setAttribute('aria-hidden', 'true');
        host.style.position = 'absolute';
        host.style.pointerEvents = 'none';
        document.body.appendChild(host);
        return host;
    }

    /**
     * motionBlur(el, options) -> Promise
     *  dir      'x' | 'y'      axis of travel
     *  distance px (signed)    starting offset ('in') or end offset ('out')
     *  duration ms
     *  maxBlur  px             peak blur at peak velocity
     *  delay    ms
     *  mode     'in' | 'out'   'in' settles into place, 'out' accelerates away (stays hidden)
     */
    function motionBlur(el, opts) {
        const o = Object.assign(
            { dir: 'x', distance: -40, duration: 450, maxBlur: 10, delay: 0, mode: 'in' },
            opts || {}
        );
        if (!el || reduced) return Promise.resolve();

        return new Promise(resolve => {
            setTimeout(() => {
                const id = 'mb' + (++counter);
                const filter = document.createElementNS(NS, 'filter');
                filter.setAttribute('id', id);
                filter.setAttribute('x', '-40%');
                filter.setAttribute('y', '-40%');
                filter.setAttribute('width', '180%');
                filter.setAttribute('height', '180%');
                const blur = document.createElementNS(NS, 'feGaussianBlur');
                blur.setAttribute('in', 'SourceGraphic');
                blur.setAttribute('stdDeviation', '0 0');
                filter.appendChild(blur);
                getHost().appendChild(filter);

                el.style.filter = 'url(#' + id + ')';
                el.style.willChange = 'transform, opacity, filter';

                const start = performance.now();
                const isIn = o.mode === 'in';

                function frame(now) {
                    const t = Math.min(1, (now - start) / o.duration);
                    let remaining, velocity, opacity;

                    if (isIn) {
                        const ease = 1 - Math.pow(1 - t, 3);     // ease-out cubic
                        remaining = 1 - ease;                     // offset left to travel
                        velocity = Math.pow(1 - t, 2);            // ~ derivative of ease
                        opacity = 0.15 + 0.85 * ease;
                    } else {
                        const ease = Math.pow(t, 3);              // ease-in cubic
                        remaining = ease;                         // offset travelled
                        velocity = Math.pow(t, 2);
                        opacity = 1 - ease;
                    }

                    const offset = o.distance * remaining;
                    const px = o.dir === 'x' ? offset : 0;
                    const py = o.dir === 'y' ? offset : 0;
                    const b = (o.maxBlur * velocity).toFixed(2);

                    el.style.transform = 'translate(' + px + 'px,' + py + 'px)';
                    el.style.opacity = opacity;
                    blur.setAttribute('stdDeviation', o.dir === 'x' ? b + ' 0' : '0 ' + b);

                    if (t < 1) {
                        requestAnimationFrame(frame);
                    } else {
                        if (isIn) {
                            el.style.filter = '';
                            el.style.transform = '';
                            el.style.opacity = '';
                            el.style.willChange = '';
                            filter.remove();
                        }
                        resolve();
                    }
                }
                requestAnimationFrame(frame);
            }, o.delay);
        });
    }

    window.motionBlur = motionBlur;
})();
