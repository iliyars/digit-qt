"""Учебные реализации. Не обёртки над DLL и не замена измерительному ПО.

Фаза — радианы, orders — phi/(2*pi). Синтетическая система: X вправо,
Y вверх, массивы показываются с origin='lower'. У Qt координата Y другая.
"""
from heapq import heappop, heappush
from math import factorial

import matplotlib.pyplot as plt
import numpy as np
from scipy import ndimage, optimize, signal

plt.rcParams.update({'figure.figsize': (9, 3.6), 'font.size': 10,
                     'axes.grid': False, 'figure.dpi': 95})
TAU = 2 * np.pi


def grid(n=128):
    axis = np.linspace(-1, 1, n)
    return np.meshgrid(axis, axis)


def scene(n=128, carrier_x=12, carrier_y=3, bowl=.3, astig=.08):
    x, y = grid(n)
    r2 = x*x + y*y
    mask = r2 < .9**2
    orders = carrier_x*x/2 + carrier_y*y/2 + bowl*r2 + astig*(x*x-y*y)
    phi = TAU*orders
    a = .52 + .04*x
    b = .32*(1-.15*r2)
    image = np.where(mask, a+b*np.cos(phi), a)
    return dict(x=x, y=y, mask=mask, orders=orders, phi=phi, a=a, b=b, image=image)


def camera(image, photons=20000, read_electrons=3, bits=12, seed=7):
    """Упрощённая модель: Poisson + шум считывания + ограничение ADC."""
    rng = np.random.default_rng(seed)
    electrons = rng.poisson(np.maximum(image, 0)*photons)
    electrons = electrons + rng.normal(0, read_electrons, image.shape)
    levels = 2**bits - 1
    return np.rint(np.clip(electrons/photons, 0, 1)*levels)/levels


def show_maps(arrays, titles, mask=None, cmap='viridis', units=''):
    fig, axes = plt.subplots(1, len(arrays), figsize=(4*len(arrays), 3.5), squeeze=False)
    for ax, array, title in zip(axes[0], arrays, titles):
        data = np.asarray(array, dtype=float)
        if mask is not None:
            data = np.where(mask, data, np.nan)
        im = ax.imshow(data, origin='lower', extent=(-1, 1, -1, 1), cmap=cmap)
        ax.set(title=title, xlabel='X', ylabel='Y')
        fig.colorbar(im, ax=ax, shrink=.85, label=units)
    fig.tight_layout()
    plt.show()


def wrap(phi):
    return np.angle(np.exp(1j*phi))


def compare(predicted, truth, mask, remove='piston'):
    """Единицы ошибки совпадают с входом. Маска и удаляемые моды явные."""
    valid = mask & np.isfinite(predicted) & np.isfinite(truth)
    if not valid.any():
        raise ValueError('Нет общих валидных точек')
    difference = np.full_like(truth, np.nan, dtype=float)
    d = (predicted-truth)[valid]
    bias = float(d.mean())
    if remove == 'piston':
        d = d-d.mean()
    elif remove == 'plane':
        x, y = grid(truth.shape[0])
        A = np.column_stack([np.ones(valid.sum()), x[valid], y[valid]])
        d = d-A@np.linalg.lstsq(A, d, rcond=None)[0]
    elif remove is not None:
        raise ValueError('remove: None, piston или plane')
    difference[valid] = d
    stats = dict(rms=float(np.sqrt(np.mean(d*d))), pv=float(np.ptp(d)), bias=bias,
                 coverage=float(valid.sum()/mask.sum()))
    return difference, stats


def unwrap_quality(wrapped, quality, mask):
    """Обход по качеству; компоненты НЕ согласуются по неизвестному порядку."""
    valid = mask & np.isfinite(wrapped) & np.isfinite(quality)
    out = np.full(wrapped.shape, np.nan)
    labels = np.zeros(wrapped.shape, dtype=int)
    h, w = wrapped.shape
    indices = np.flatnonzero(valid)
    indices = indices[np.argsort(quality.ravel()[indices])[::-1]]
    region = 0
    for index in indices:
        y, x = divmod(int(index), w)
        if labels[y, x]:
            continue
        region += 1
        out[y, x] = wrapped[y, x]
        labels[y, x] = region
        queue = []

        def add_neighbors(y0, x0):
            for dy, dx in [(0, 1), (1, 0), (0, -1), (-1, 0)]:
                ny, nx = y0+dy, x0+dx
                if 0 <= ny < h and 0 <= nx < w and valid[ny, nx] and not labels[ny, nx]:
                    q = min(quality[y0, x0], quality[ny, nx])
                    heappush(queue, (-q, y0, x0, ny, nx))

        add_neighbors(y, x)
        while queue:
            _, sy, sx, ny, nx = heappop(queue)
            if labels[ny, nx]:
                continue
            out[ny, nx] = out[sy, sx]+wrap(wrapped[ny, nx]-wrapped[sy, sx])
            labels[ny, nx] = region
            add_neighbors(ny, nx)
    return out, labels


def fourier_phase(image, mask, sigma_bins=None):
    """Учебный пространственный фильтр; требуются разделённые боковые полосы."""
    h, w = image.shape
    centered = image-float(image[mask].mean())
    if np.std(centered[mask]) < 1e-10:
        raise ValueError('Нет измеримого контраста')
    distance = ndimage.distance_transform_edt(mask)
    taper = .5-.5*np.cos(np.pi*np.minimum(distance/8, 1))
    spectrum = np.fft.fftshift(np.fft.fft2(centered*taper*mask))
    fx = np.fft.fftshift(np.fft.fftfreq(w))
    fy = np.fft.fftshift(np.fft.fftfreq(h))
    FX, FY = np.meshgrid(fx, fy)
    positive = (FX > 0) | ((FX == 0) & (FY > 0))
    allowed = positive & (np.hypot(FX*w, FY*h) > 3)
    magnitude = np.abs(spectrum)
    iy, ix = np.unravel_index(np.argmax(np.where(allowed, magnitude, -1)), image.shape)
    cx, cy = fx[ix], fy[iy]
    distance_bins = np.hypot(cx*w, cy*h)
    if sigma_bins is None:
        sigma_bins = .24*distance_bins
    if sigma_bins <= 0:
        raise ValueError('Положительная ширина фильтра обязательна')
    window = np.exp(-((FX-cx)**2*w*w+(FY-cy)**2*h*h)/(2*sigma_bins**2))
    window *= positive
    filtered = spectrum*window
    analytic = np.fft.ifft2(np.fft.ifftshift(filtered))
    yy, xx = np.indices(image.shape)
    carrier = TAU*(cx*xx+cy*yy)
    wrapped = np.angle(analytic*np.exp(-1j*carrier))
    quality = np.abs(analytic)
    valid = mask & (quality > .05*np.median(quality[mask]))
    unwrapped, labels = unwrap_quality(wrapped, quality, valid)
    return dict(phi=unwrapped+carrier, quality=quality, labels=labels,
                spectrum=spectrum, window=window, frequency=(cx, cy), valid=valid)


def cwt_phase(image, mask, periods, axis=1, omega=5.5):
    """1D комплексный вейвлет по выбранной оси; не полный 2D CWT."""
    if axis == 0:
        phi, quality = cwt_phase(image.T, mask.T, periods, axis=1, omega=omega)
        return phi.T, quality.T
    centered = np.where(mask, image-image[mask].mean(), 0)
    coefficients = []
    for period in periods:
        scale = omega*period/TAU
        support = int(np.ceil(4*scale))
        u = np.arange(-support, support+1)/scale
        kernel = np.exp(-u*u/2)*np.exp(1j*omega*u)
        kernel -= kernel.mean()  # нулевое среднее конечного ядра
        kernel /= np.sqrt(scale)
        coefficients.append(signal.fftconvolve(centered, kernel[None, :], mode='same', axes=1))
    coefficients = np.stack(coefficients)
    ridge = np.argmax(np.abs(coefficients), axis=0)
    selected = np.take_along_axis(coefficients, ridge[None], axis=0)[0]
    quality = np.abs(selected)
    valid = mask & (quality > .05*np.median(quality[mask]))
    phi, _ = unwrap_quality(np.angle(selected), quality, valid)
    return phi, quality


def psi_known(stack, shifts, mask):
    stack, shifts = np.asarray(stack), np.asarray(shifts)
    D = np.column_stack([np.ones(len(shifts)), np.cos(shifts), -np.sin(shifts)])
    coefficients, _, rank, singular = np.linalg.lstsq(D, stack[:, mask], rcond=None)
    if rank < 3:
        raise ValueError('Сдвиги не позволяют определить три неизвестных')
    a, c, s = coefficients
    phi = np.full(mask.shape, np.nan)
    phi[mask] = np.arctan2(s, c)
    return dict(phi=phi, a=a, b=np.hypot(c, s), coefficients=coefficients,
                residual=stack[:, mask]-D@coefficients,
                condition=float(singular[0]/singular[-1]))


def psi_unknown(stack, initial, mask, max_iterations=100, tolerance=1e-7):
    """Чередование LS по пикселям и минимизации по каждому delta_n.

    Сохраняет пространственные a(x,y), b(x,y). Это учебная альтернативная
    постановка, а не копия шага оценки сдвигов в DigitQt.
    """
    shifts = np.asarray(initial, dtype=float).copy()
    shifts -= shifts[0]
    history = []
    for _ in range(max_iterations):
        fitted = psi_known(stack, shifts, mask)
        a, c, s = fitted['coefficients']
        p, q = c, -s
        pp, qq, pq = p@p, q@q, p@q
        new = shifts.copy()
        for n in range(1, len(shifts)):
            target = stack[n, mask]-a
            tp, tq = target@p, target@q

            def objective(delta):
                co, si = np.cos(delta), np.sin(delta)
                return pp*co*co+qq*si*si+2*pq*co*si-2*tp*co-2*tq*si

            angles = np.linspace(-np.pi, np.pi, 64, endpoint=False)
            center = angles[np.argmin([objective(d) for d in angles])]
            fit = optimize.minimize_scalar(objective, bounds=(center-.12, center+.12), method='bounded')
            new[n] = fit.x
        change = np.sqrt(np.mean(wrap(new-shifts)**2))
        shifts = new
        history.append(float(np.mean(psi_known(stack, shifts, mask)['residual']**2)))
        if change < tolerance:
            break
    result = psi_known(stack, shifts, mask)
    result.update(shifts=shifts, history=np.array(history), converged=change < tolerance)
    return result


def psi_planes(stack, initial_shifts, x, y, mask, max_evaluations=45):
    """Variable projection: внешние параметры — плоскости сдвигов,
    внутренние — линейные a,c,s каждого пикселя. Не транскрипция Liu.
    Наклоны в rad на единицу нормированной координаты, не rad/pixel.
    """
    data = stack[:, mask].T
    X, Y = x[mask], y[mask]
    frame_count = stack.shape[0]

    def inner(parameters):
        planes = np.vstack([np.zeros(3), parameters.reshape(-1, 3)])
        delta = planes[:, 0, None]+planes[:, 1, None]*X+planes[:, 2, None]*Y
        D = np.stack([np.ones_like(delta), np.cos(delta), -np.sin(delta)], axis=-1).transpose(1, 0, 2)
        U, singular, VT = np.linalg.svd(D, full_matrices=False)
        projected = np.einsum('pmk,pm->pk', U, data)
        coefficients = np.einsum('pki,pk->pi', VT, projected/np.maximum(singular, 1e-12))
        residual = np.einsum('pmk,pk->pm', D, coefficients)-data
        return coefficients, residual, planes

    start = np.zeros((frame_count-1, 3))
    start[:, 0] = np.asarray(initial_shifts)[1:]-initial_shifts[0]
    fit = optimize.least_squares(lambda p: inner(p)[1].ravel(), start.ravel(),
                                 max_nfev=max_evaluations, x_scale='jac')
    coefficients, residual, planes = inner(fit.x)
    phi = np.full(mask.shape, np.nan)
    phi[mask] = np.arctan2(coefficients[:, 2], coefficients[:, 1])
    return dict(phi=phi, planes=planes, residual=residual, optimization=fit)


def tensor_orientation(image, sigma=1.5):
    gy, gx = np.gradient(image)
    xx = ndimage.gaussian_filter(gx*gx, sigma)
    yy = ndimage.gaussian_filter(gy*gy, sigma)
    xy = ndimage.gaussian_filter(gx*gy, sigma)
    normal = .5*np.arctan2(2*xy, xx-yy)
    coherence = np.hypot(xx-yy, 2*xy)/(xx+yy+1e-12)
    return normal, coherence


def row_tracker(image, seed_x, seed_y, radius=5):
    """Учебное продолжение максимума по строкам; не legacy FTM."""
    points = [(float(seed_x), float(seed_y))]
    x = int(seed_x)
    for y in range(seed_y+1, image.shape[0]):
        lo, hi = max(1, x-radius), min(image.shape[1]-1, x+radius+1)
        if lo >= hi:
            break
        x = lo+int(np.argmax(image[y, lo:hi]))
        a, b, c = image[y, x-1:x+2]
        denominator = a-2*b+c
        subpixel = .5*(a-c)/denominator if abs(denominator)>1e-12 else 0
        points.append((x+float(np.clip(subpixel, -.5, .5)), float(y)))
    return np.array(points)


def thin_zhang_suen(binary, max_iterations=120):
    """Две подитерации удаления граничных пикселей, без обрезки/склейки."""
    image = np.pad(binary.astype(bool), 1)
    for _ in range(max_iterations):
        changed = False
        for step in (0, 1):
            p2,p3,p4,p5 = image[:-2,1:-1],image[:-2,2:],image[1:-1,2:],image[2:,2:]
            p6,p7,p8,p9 = image[2:,1:-1],image[2:,:-2],image[1:-1,:-2],image[:-2,:-2]
            ring = [p2,p3,p4,p5,p6,p7,p8,p9,p2]
            neighbors = sum(p.astype(int) for p in ring[:-1])
            changes = sum((~a & b).astype(int) for a,b in zip(ring[:-1],ring[1:]))
            c1 = ~(p2&p4&(p6 if step==0 else p8))
            c2 = ~((p4 if step==0 else p2)&p6&p8)
            remove = image[1:-1,1:-1] & (neighbors>=2) & (neighbors<=6) & (changes==1) & c1 & c2
            if remove.any():
                changed = True
                image[1:-1,1:-1][remove] = False
        if not changed:
            break
    return image[1:-1,1:-1]


TERMS = [(0,0),(1,1),(1,-1),(2,0),(2,2),(2,-2),(3,1),(3,-1),(3,3),(3,-3),(4,0)]


def zernike(n, m, x, y, normalization='unit'):
    if abs(m)>n or (n-abs(m))%2:
        raise ValueError('Требуются |m|<=n и чётное n-|m|')
    radius = np.hypot(x,y)
    theta = np.arctan2(y,x)
    radial = np.zeros_like(radius)
    for k in range((n-abs(m))//2+1):
        coefficient = (-1)**k*factorial(n-k)/(factorial(k)*factorial((n+abs(m))//2-k)*factorial((n-abs(m))//2-k))
        radial += coefficient*radius**(n-2*k)
    angular = np.cos(m*theta) if m>=0 else np.sin(abs(m)*theta)
    value = radial*angular
    if normalization == 'rms':
        value *= np.sqrt(n+1 if m==0 else 2*(n+1))
    elif normalization != 'unit':
        raise ValueError('Нормировка: unit или rms')
    return value


def modal_fit(surface, x, y, mask, terms=TERMS, normalization='unit'):
    A = np.column_stack([zernike(n,m,x,y,normalization)[mask] for n,m in terms])
    coefficients, _, rank, singular = np.linalg.lstsq(A, surface[mask], rcond=None)
    if rank < len(terms):
        raise ValueError('Термы не независимы на данной апертуре')
    fitted = np.full_like(surface, np.nan)
    fitted[mask] = A@coefficients
    return dict(coefficients=coefficients, fitted=fitted, residual=surface-fitted,
                matrix=A, singular=singular, condition=singular[0]/singular[-1])


def sequential_seregin(surface, x, y, mask):
    """Учебная последовательность проекций в явных формулах, без
    эмпирических множителей отчёта. Y — математическая, вверх.
    """
    X,Y,z = x[mask],y[mask],surface[mask]
    one = np.ones_like(X)
    r2 = X*X+Y*Y
    stages = [([one,X,Y], [0,1,2]), ([one,r2], [0,1]),
              ([one,X,Y,(3*X*X-1)/2,X*Y,(3*Y*Y-1)/2], [0,3,4,5]),
              ([one,X,Y,X*(r2-2/3),Y*(r2-2/3),3*X*Y*Y-X**3,3*X*X*Y-Y**3], list(range(7))),
              ([one,r2,r2**2], [0,1,2]), ([one,r2,r2**2,r2**3], [0,1,2,3]),
              ([one,r2,r2**2,r2**3,r2**4], [0,1,2,3,4])]
    fitted = np.zeros_like(z)
    history, coefficients = [], []
    for columns, retained in stages:
        A = np.column_stack(columns)
        c, _, rank, _ = np.linalg.lstsq(A, z-fitted, rcond=None)
        if rank < len(columns):
            raise ValueError('Вырожденный этап последовательной схемы')
        fitted += A[:,retained]@c[retained]
        coefficients.append(c)
        history.append(np.sqrt(np.mean((z-fitted)**2)))
    residual = np.full_like(surface, np.nan)
    residual[mask] = z-fitted
    return dict(residual=residual, history=np.array(history), coefficients=coefficients)
