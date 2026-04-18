import { test, expect } from '@playwright/test';

/**
 * Clockwise Paradise WebUI — E2E test suite
 *
 * Tests the multi-page WebUI introduced in v2.3.0.
 * Runs against a live device; set CW_DEVICE_URL to override the target.
 *
 * Pages tested:
 *   GET /           → Home (status dashboard)
 *   GET /clock      → Clock settings
 *   GET /sync       → Sync & Connectivity
 *   GET /hardware   → Hardware settings
 *   GET /update     → Firmware update
 *   GET /get        → Settings API headers
 *   POST /set       → Save a setting
 *   POST /ota/check → OTA check API
 */

// ─── Helpers ────────────────────────────────────────────────────────────────

/** Small pause between tests — ESP32 is single-threaded, needs recovery time */
test.beforeEach(async () => {
  await new Promise(r => setTimeout(r, 400));
});

/** Fetch /get and return headers as lowercase key→value map */
async function getHeaders(baseURL: string): Promise<Record<string, string>> {
  const res = await fetch(`${baseURL}/get`);
  const headers: Record<string, string> = {};
  res.headers.forEach((v, k) => {
    headers[k.toLowerCase().replace(/^x-/, '')] = v;
  });
  return headers;
}

// ─── API sanity ──────────────────────────────────────────────────────────────

test.describe('GET /get — settings API', () => {
  test('responds with 204 and required headers', async ({ baseURL }) => {
    const res = await fetch(`${baseURL}/get`);
    expect(res.status).toBe(204);
  });

  test('returns firmware version header', async ({ baseURL }) => {
    const h = await getHeaders(baseURL!);
    expect(h['cw_fw_version']).toBeTruthy();
    expect(h['cw_fw_version']).not.toBe('dev');
  });

  test('returns device IP header', async ({ baseURL }) => {
    const h = await getHeaders(baseURL!);
    expect(h['wifiip']).toMatch(/^\d+\.\d+\.\d+\.\d+$/);
  });

  test('returns WiFi SSID header', async ({ baseURL }) => {
    const h = await getHeaders(baseURL!);
    expect(h['wifissid']).toBeTruthy();
  });

  test('timezone value is not URL-encoded', async ({ baseURL }) => {
    const h = await getHeaders(baseURL!);
    const tz = h['timezone'] || '';
    expect(tz).not.toContain('%2F');
    expect(tz).not.toContain('%');
  });

  test('driver header present and numeric', async ({ baseURL }) => {
    const h = await getHeaders(baseURL!);
    expect(Number(h['driver'])).toBeGreaterThanOrEqual(0);
  });
});

// ─── Home page ───────────────────────────────────────────────────────────────

test.describe('GET / — Home page', () => {
  test('page title is correct', async ({ page }) => {
    await page.goto('/');
    await expect(page).toHaveTitle('Clockwise Paradise');
  });

  test('header shows correct firmware version', async ({ page, baseURL }) => {
    const h = await getHeaders(baseURL!);
    await page.goto('/');
    await page.waitForTimeout(2000); // wait for JS to populate
    const ver = await page.locator('#hdr-ver').textContent();
    expect(ver).toContain(h['cw_fw_version']);
    expect(ver).not.toContain('dev');
    expect(ver).not.toContain('Loading');
  });

  test('status dashboard shows device IP', async ({ page }) => {
    await page.goto('/');
    await page.waitForTimeout(2000);
    const wifi = await page.locator('#wifi-info').textContent();
    expect(wifi).toMatch(/\d+\.\d+\.\d+\.\d+/);
  });

  test('status dashboard shows firmware name', async ({ page }) => {
    await page.goto('/');
    await page.waitForTimeout(2000);
    const fw = await page.locator('#fw-ver').textContent();
    expect(fw).toContain('Clockwise Paradise');
    expect(fw).not.toBe('…');
  });

  test('nav links present for all pages', async ({ page }) => {
    await page.goto('/');
    for (const href of ['/clock', '/sync', '/hardware', '/update']) {
      await expect(page.locator(`nav a[href="${href}"], .nav a[href="${href}"]`)).toBeVisible();
    }
  });
});

// ─── Clock page ──────────────────────────────────────────────────────────────

test.describe('GET /clock — Clock settings', () => {
  test('page loads without JS errors', async ({ page }) => {
    const errors: string[] = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.goto('/clock');
    await page.waitForTimeout(2000);
    expect(errors).toHaveLength(0);
  });

  test('brightness slider is populated', async ({ page }) => {
    await page.goto('/clock');
    await page.waitForTimeout(2000);
    const val = await page.locator('#displayBright').inputValue();
    expect(Number(val)).toBeGreaterThanOrEqual(0);
    expect(Number(val)).toBeLessThanOrEqual(255);
  });

  test('Apply button exists', async ({ page }) => {
    await page.goto('/clock');
    await expect(page.locator('button:has-text("Apply")')).toBeVisible();
  });

  test('clockface selector is populated', async ({ page }) => {
    await page.goto('/clock');
    await page.waitForTimeout(2000);
    const options = await page.locator('#clockFace option').count();
    expect(options).toBeGreaterThan(0);
  });
});

// ─── Sync page ───────────────────────────────────────────────────────────────

test.describe('GET /sync — Sync & Connectivity', () => {
  test('page loads without JS errors', async ({ page }) => {
    const errors: string[] = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.goto('/sync');
    await page.waitForTimeout(2000);
    expect(errors).toHaveLength(0);
  });

  test('timezone field populated and not URL-encoded', async ({ page }) => {
    await page.goto('/sync');
    await page.waitForTimeout(2000);
    const tz = await page.locator('#timeZone').inputValue();
    expect(tz).not.toContain('%2F');
    expect(tz).not.toContain('%');
    expect(tz.length).toBeGreaterThan(0);
  });

  test('HA Discovery toggle is disabled (not yet implemented)', async ({ page }) => {
    await page.goto('/sync');
    const disabled = await page.locator('#haDiscovery').isDisabled();
    expect(disabled).toBe(true);
  });

  test('MQTT port defaults to 1883', async ({ page }) => {
    await page.goto('/sync');
    await page.waitForTimeout(2000);
    const port = await page.locator('#mqttPort').inputValue();
    expect(port).toBe('1883');
  });

  test('MQTT password updates keep the secret out of the URL', async ({ page }) => {
    let requestUrl = '';
    let requestBody = '';
    await page.route('**/set?mqttPass=', async route => {
      requestUrl = route.request().url();
      requestBody = route.request().postData() || '';
      await route.fulfill({ status: 204, body: '' });
    });

    await page.goto('/sync');
    await page.waitForTimeout(2000);
    await page.evaluate(() => setKey('mqttPass', 'topsecret123'));

    expect(requestUrl).toContain('/set?mqttPass=');
    expect(requestUrl).not.toContain('topsecret123');
    expect(requestBody).toBe('value=topsecret123');
  });

  test('non-sensitive sync updates still use query-string values', async ({ page }) => {
    let requestUrl = '';
    let requestBody = '';
    await page.route('**/set?timeZone=*', async route => {
      requestUrl = route.request().url();
      requestBody = route.request().postData() || '';
      await route.fulfill({ status: 204, body: '' });
    });

    await page.goto('/sync');
    await page.waitForTimeout(2000);
    await page.evaluate(() => setKey('timeZone', 'America/New_York'));

    expect(requestUrl).toContain('/set?timeZone=America%2FNew_York');
    expect(requestBody).toBe('');
  });
});

// ─── Hardware page ───────────────────────────────────────────────────────────

test.describe('GET /hardware — Hardware settings', () => {
  test('page loads without JS errors', async ({ page }) => {
    const errors: string[] = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.goto('/hardware');
    await page.waitForTimeout(2000);
    expect(errors).toHaveLength(0);
  });

  test('driver selector is populated from device', async ({ page }) => {
    await page.goto('/hardware');
    await page.waitForTimeout(2000);
    const driver = await page.locator('#driver').inputValue();
    expect(['0', '1', '2', '3', '4', '5']).toContain(driver);
  });

  test('Apply button exists', async ({ page }) => {
    await page.goto('/hardware');
    await expect(page.locator('button:has-text("Apply")')).toBeVisible();
  });
});

// ─── Update page ─────────────────────────────────────────────────────────────

test.describe('GET /update — Firmware update', () => {
  test('page loads without JS errors', async ({ page }) => {
    const errors: string[] = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.goto('/update');
    await page.waitForTimeout(2000);
    expect(errors).toHaveLength(0);
  });

  test('version badge shows current version', async ({ page, baseURL }) => {
    const h = await getHeaders(baseURL!);
    await page.goto('/update');
    await page.waitForTimeout(2000);
    const ver = await page.locator('#ver').textContent();
    expect(ver).toContain(h['cw_fw_version']);
  });

  test('upload file input accepts .bin', async ({ page }) => {
    await page.goto('/update');
    const accept = await page.locator('#bin').getAttribute('accept');
    expect(accept).toContain('.bin');
  });
});

// ─── OTA check API ───────────────────────────────────────────────────────────

test.describe('GET /ota/check — OTA check API', () => {
  test('responds with JSON', async ({ baseURL }) => {
    const res = await fetch(`${baseURL}/ota/check`);
    const ct = res.headers.get('content-type') || '';
    expect(ct).toContain('application/json');
  });

  test('response contains available field or error status', async ({ baseURL }) => {
    const res = await fetch(`${baseURL}/ota/check`);
    const json = await res.json();
    // Device may not have internet access in all environments;
    // accept either a successful check or a graceful error response
    const hasAvailable = 'available' in json;
    const hasStatus = 'status' in json;
    expect(hasAvailable || hasStatus).toBe(true);
  });
});

// ─── Legacy page ─────────────────────────────────────────────────────────────

test.describe('GET /legacy — legacy UI fallback', () => {
  test('page exists and returns 200', async ({ page }) => {
    const res = await page.goto('/legacy');
    expect(res?.status()).toBe(200);
  });
});
