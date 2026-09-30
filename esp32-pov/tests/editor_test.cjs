const assert = require("node:assert/strict");
const fs = require("node:fs");
const { JSDOM } = require("jsdom");
let state = {
  on: false,
  ready: true,
  mode: "pov",
  lamp: { preset: 0, ledCount: 15, brightness: 20, color: "ffb347" },
  ledCount: 11,
  columns: 10,
  columnMs: 5,
  brightness: 20,
  pixels: "000000".repeat(110),
};
const requests = [];
const html = fs.readFileSync(
  require("node:path").join(__dirname, "../editor.html"),
  "utf8",
);
const dom = new JSDOM(html, {
  url: "http://lamp/editor",
  runScripts: "dangerously",
  beforeParse(w) {
    w.AbortSignal = AbortSignal;
    w.confirm = () => true;
    w.fetch = async (path, options) => {
      requests.push({ path, body: options.body });
      if (options.method === "POST") {
        if (path === "/api/pattern") {
          const [header, pixels] = options.body.split("\n");
          const [ledCount, columns, columnMs, brightness] = header
            .split(",")
            .map(Number);
          state = {
            ...state,
            ledCount,
            columns,
            columnMs,
            brightness,
            pixels,
            on: true,
            mode: "pov",
          };
        } else if (path === "/api/power") state.on = options.body === "on";
        else if (path === "/api/mode") state.mode = options.body;
        else if (path === "/api/lamp") {
          const [header, color] = options.body.split("\n");
          const [preset, ledCount, brightness] = header.split(",").map(Number);
          state.lamp = { preset, ledCount, brightness, color };
          state.mode = "lamp";
          state.on = true;
        }
      }
      return {
        ok: true,
        json: async () => JSON.parse(JSON.stringify(state)),
        text: async () => "",
      };
    };
  },
});
const doc = dom.window.document;
const wait = () => new Promise((resolve) => setImmediate(resolve));
(async () => {
  await wait();
  assert.equal(doc.querySelectorAll(".cell").length, 110);
  assert.equal(doc.getElementById("send").disabled, false);
  doc.querySelector('[data-pixel="0"]').click();
  doc.querySelector('[aria-label="Blue"]').click();
  doc.querySelector('[data-pixel="11"]').click();
  doc.getElementById("addColumn").click();
  assert.equal(doc.querySelectorAll(".cell").length, 121);
  doc.getElementById("ledCount").value = "10";
  doc.getElementById("ledCount").dispatchEvent(new dom.window.Event("change"));
  assert.equal(doc.querySelectorAll(".cell").length, 110);
  doc.getElementById("send").click();
  await wait();
  const upload = requests.find((r) => r.path === "/api/pattern" && r.body);
  assert(upload.body.startsWith("10,11,5,20\nff5d5d"));
  assert.equal(upload.body.split("\n")[1].slice(60, 66), "5982ff");
  assert.equal(doc.getElementById("saved").textContent, "Saved on lamp");
  assert.equal(
    doc.getElementById("power").getAttribute("aria-checked"),
    "true",
  );
  doc.getElementById("power").click();
  await wait();
  assert.equal(state.on, false);
  assert.equal(doc.getElementById("powerLabel").textContent, "Off");
  const savedPixels = state.pixels;
  doc.getElementById("lampMode").click();
  await wait();
  assert.equal(state.mode, "lamp");
  assert.equal(doc.getElementById("povPanel").hidden, true);
  assert.equal(doc.getElementById("lampPanel").hidden, false);
  assert.equal(doc.querySelectorAll(".lamp-led").length, 15);
  assert.equal(doc.querySelectorAll(".preset").length, 5);
  doc.querySelector('[aria-label="Fireplace"]').click();
  assert.equal(state.lamp.preset, 0, "preview must not change saved settings");
  doc.getElementById("applyLamp").click();
  await wait();
  assert.equal(state.lamp.preset, 1);
  assert.equal(state.on, true);
  assert.equal(doc.getElementById("powerLabel").textContent, "On");
  doc.getElementById("lampColor").value = "#12abcd";
  doc.getElementById("lampColor").dispatchEvent(new dom.window.Event("input"));
  doc.getElementById("applyLamp").click();
  await wait();
  assert.equal(state.lamp.preset, 0, "custom color selects solid mode");
  assert.equal(state.lamp.color, "12abcd");
  doc.getElementById("povMode").click();
  await wait();
  assert.equal(state.mode, "pov");
  assert.equal(
    state.pixels,
    savedPixels,
    "lamp mode must preserve the POV picture",
  );
  assert.equal(doc.getElementById("povPanel").hidden, false);
  assert.equal(doc.getElementById("lampPanel").hidden, true);
  dom.window.close();
  console.log(
    "PASS: POV editing, upload, power, lamp presets, custom color, mode switching preserves picture",
  );
})().catch((error) => {
  dom.window.close();
  console.error(error);
  process.exitCode = 1;
});
