#pragma once
static const char DASHBOARD_HTML[] PROGMEM = R"HTMLEND(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#000000">
<meta name="color-scheme" content="dark">
<title>ETA Heizung</title>
<style>
:root{
--y:#f5a800;--y2:#ffc84a;--yd:#b57c00;
--bg:#0a0a0a;--pnl1:#2b2b2b;--pnl2:#131313;--box:#2e2e2e;--brd:#585858;
--tx:#f2f2f2;--tx2:#9d9d9d;--val:#6cc0ee;--red:#e03a2f;--grn:#76b82a;
}
*{box-sizing:border-box;margin:0;padding:0}
html,body{height:100%}
body{background:var(--bg);color:var(--tx);font-family:'Segoe UI',Roboto,Arial,sans-serif;font-size:15px;
 -webkit-tap-highlight-color:transparent;display:flex;flex-direction:column;min-height:100%}

/* ---------- Kopfleiste mit Registern ---------- */
#hd{display:flex;align-items:stretch;background:#000;border-bottom:2px solid var(--y);position:sticky;top:0;z-index:30}
.logo{flex:0 0 auto;display:flex;align-items:center;justify-content:center;padding:0 14px;background:#000;
 border-right:1px solid #444}
.logo b{border:2px solid #fff;border-radius:3px;padding:1px 7px;font-size:1rem;font-weight:800;letter-spacing:2px;
 font-family:Arial,sans-serif;color:#fff}
#nav{display:flex;overflow-x:auto;flex:1;scrollbar-width:none}
#nav::-webkit-scrollbar{display:none}
#nav button{flex:0 0 auto;background:linear-gradient(180deg,#4a4a4a,#242424);border:none;border-right:1px solid #0c0c0c;
 color:#e8e8e8;font:inherit;font-size:.86rem;padding:12px 13px;cursor:pointer;white-space:nowrap;
 text-shadow:0 1px 1px rgba(0,0,0,.6)}
#nav button:hover{background:linear-gradient(180deg,#5f5f5f,#333)}
#nav button.act{background:linear-gradient(180deg,var(--y2),var(--y));color:#111;font-weight:700;text-shadow:none;
 box-shadow:inset 0 -2px 0 rgba(0,0,0,.25)}
.hq{flex:0 0 auto;background:linear-gradient(180deg,#4a4a4a,#242424);border:none;border-left:1px solid #0c0c0c;
 color:var(--y);font-size:1.3rem;font-weight:800;padding:0 14px;cursor:pointer}

/* ---------- Rahmen und Bühne ---------- */
main{flex:1;padding:10px;max-width:1180px;width:100%;margin:0 auto;display:flex;flex-direction:column}
.frame{flex:1;border:2px solid var(--y);border-radius:9px;
 background:radial-gradient(120% 90% at 50% 0%,#343434,#101010 62%,#080808);
 padding:12px;box-shadow:0 0 0 1px #000 inset,0 8px 26px rgba(0,0,0,.6)}
.sbar{border:1px solid var(--yd);border-radius:8px;background:linear-gradient(180deg,#ffd268,var(--y) 45%,#e09600);
 color:#141414;font-weight:700;font-size:1.08rem;padding:12px 18px;margin-bottom:12px;
 box-shadow:inset 0 1px 0 rgba(255,255,255,.45),0 2px 6px rgba(0,0,0,.5)}
.sbar.err{background:linear-gradient(180deg,#f4675b,var(--red) 45%,#b81f16);border-color:#8d1a12;color:#fff;
 text-shadow:0 1px 1px rgba(0,0,0,.4)}
.sbar.off{background:linear-gradient(180deg,#6a6a6a,#3a3a3a);border-color:#222;color:#e0e0e0}
.stage{display:flex;gap:14px;align-items:stretch}
.scene{flex:1;min-width:0;background:linear-gradient(180deg,#141414,#050505);border:1px solid #383838;
 border-radius:8px;padding:18px 12px;box-shadow:inset 0 2px 10px rgba(0,0,0,.8);
 display:flex;align-items:center;justify-content:center;gap:10px;flex-wrap:nowrap;overflow-x:auto;min-height:230px}
.side{flex:0 0 138px;display:flex;flex-direction:column;gap:16px;align-items:center;justify-content:center}

/* ---------- Wertekästchen ---------- */
.vcol{display:flex;flex-direction:column;gap:10px}
.vcol .vb{min-width:96px}
.vb{position:relative;background:linear-gradient(180deg,#454545,#1d1d1d);border:1px solid #6b6b6b;
 border-radius:6px;padding:6px 11px;font-size:1.04rem;font-weight:700;color:#fff;white-space:nowrap;text-align:center;
 box-shadow:inset 0 1px 0 rgba(255,255,255,.14),0 2px 5px rgba(0,0,0,.55)}
.vb s{color:var(--y);font-size:.8rem;font-weight:600;text-decoration:none;margin-left:3px}
.vb em{display:block;font-style:normal;font-size:.68rem;color:#a8a8a8;font-weight:400;letter-spacing:.3px}
.vcol.l .vb:after,.vcol.r .vb:after{content:'';position:absolute;top:50%;width:18px;height:1px;background:var(--y);
 box-shadow:0 0 3px rgba(245,168,0,.7)}
.vcol.l .vb:before,.vcol.r .vb:before{content:'';position:absolute;top:50%;width:5px;height:5px;margin-top:-2px;
 border-radius:50%;background:var(--y)}
.vcol.l .vb:after{right:-18px}
.vcol.l .vb:before{right:-20px}
.vcol.r .vb:after{left:-18px}
.vcol.r .vb:before{left:-20px}

/* ---------- Bauteil-Grafiken ---------- */
.gfx{flex:0 0 auto;position:relative}
.cap{text-align:center;color:#a8a8a8;font-size:.72rem;margin-top:7px;letter-spacing:.3px}
.shine{position:absolute;inset:0;border-radius:inherit;pointer-events:none;
 background:linear-gradient(90deg,rgba(0,0,0,.45),rgba(255,255,255,.26) 22%,rgba(255,255,255,.04) 54%,rgba(0,0,0,.5))}
.kessel{flex:0 0 auto;width:112px;height:156px;border-radius:6px;position:relative;
 background:linear-gradient(90deg,#2d2d2d,#9a9a9a 24%,#d4d4d4 40%,#6e6e6e 76%,#333);
 box-shadow:0 6px 18px rgba(0,0,0,.75),inset 0 0 0 1px rgba(255,255,255,.16)}
.kessel:before{content:'';position:absolute;left:5px;right:5px;top:-8px;height:9px;border-radius:4px 4px 0 0;
 background:linear-gradient(180deg,#c6c6c6,#5a5a5a)}
.kessel i{position:absolute;right:7px;top:11px;bottom:11px;width:52px;border-radius:5px;
 background:linear-gradient(180deg,#ffd268,#bd7c00);
 box-shadow:inset 0 1px 0 rgba(255,255,255,.55),0 2px 5px rgba(0,0,0,.6)}
.kessel i:before{content:'';position:absolute;left:8px;right:8px;top:16px;bottom:16px;border-radius:24px;
 background:#191919;box-shadow:inset 0 0 12px #000,0 0 0 1px rgba(0,0,0,.6)}
.kessel.on i:before{background:radial-gradient(circle at 50% 76%,#ffbe3a,#e2540a 40%,#712600 74%,#1d0c00);
 box-shadow:inset 0 0 14px rgba(0,0,0,.55),0 0 16px rgba(255,120,20,.5)}
.batt{flex:0 0 auto;width:112px;height:210px;position:relative;border-radius:13px;padding:8px 6px;
 background:linear-gradient(180deg,#d0d0d0,#8d8d8d 22%,#6a6a6a 78%,#3f3f3f);
 box-shadow:0 6px 18px rgba(0,0,0,.7)}
.batt u{position:absolute;top:-8px;left:50%;transform:translateX(-50%);width:36px;height:9px;
 border-radius:3px 3px 0 0;background:linear-gradient(180deg,#dcdcdc,#8a8a8a)}
.batt i{display:block;position:relative;height:100%;border-radius:7px;overflow:hidden;
 box-shadow:inset 0 0 0 1px rgba(0,0,0,.5)}
.tk{flex:0 0 auto;width:80px;height:162px;position:relative;border-radius:15px;padding:7px 6px;
 background:linear-gradient(180deg,#cccccc,#8a8a8a 22%,#666 78%,#3d3d3d);box-shadow:0 6px 18px rgba(0,0,0,.7)}
.tk i{display:block;position:relative;height:100%;border-radius:9px;overflow:hidden;
 box-shadow:inset 0 0 0 1px rgba(0,0,0,.5)}
.rad{width:128px;height:94px;border:1px solid #4a4a4a;border-radius:5px;position:relative;
 background:repeating-linear-gradient(90deg,#e2e2e2 0 10px,#9a9a9a 10px 13px,#6e6e6e 13px 16px);
 box-shadow:0 6px 18px rgba(0,0,0,.7)}
.rad:after{content:'';position:absolute;left:0;right:0;top:0;height:9px;border-radius:5px 5px 0 0;
 background:linear-gradient(180deg,#f2f2f2,#9d9d9d)}
.coll{width:144px;height:94px;border:2px solid #b0b0b0;border-radius:4px;transform:skewX(-11deg);
 background:repeating-linear-gradient(90deg,#0e2c44 0 10px,#2f74a8 10px 14px);
 box-shadow:0 6px 18px rgba(0,0,0,.7),inset 0 0 22px rgba(0,0,0,.6)}
.silo{width:128px;height:154px;position:relative;overflow:hidden;border:2px solid #6f6f6f;border-radius:5px;
 background:linear-gradient(90deg,#101010,#282828 30%,#161616);box-shadow:0 6px 18px rgba(0,0,0,.7)}
.silo i{position:absolute;left:0;right:0;bottom:0;
 background:linear-gradient(180deg,#e2b061,#8a5a1c);box-shadow:inset 0 2px 4px rgba(255,255,255,.3)}
.ovl{position:absolute;left:50%;transform:translateX(-50%);z-index:3;white-space:nowrap;
 background:linear-gradient(180deg,#2c2c2c,#0b0b0b);border:1px solid #6e6e6e;border-radius:5px;padding:2px 8px;
 font-size:.9rem;font-weight:700;color:var(--y2);box-shadow:0 2px 6px rgba(0,0,0,.7)}
.ovl s{color:var(--y);font-size:.72rem;text-decoration:none;margin-left:2px}
.ovl.st{position:static;transform:none;display:inline-block}
.pipe{flex:0 0 auto;width:38px;height:10px;border-radius:5px;align-self:center;position:relative;
 background:linear-gradient(180deg,#b0b0b0,#5c5c5c 55%,#2c2c2c);
 box-shadow:inset 0 1px 0 rgba(255,255,255,.5),0 2px 4px rgba(0,0,0,.6)}
.pipe:before,.pipe:after{content:'';position:absolute;top:-2px;bottom:-2px;width:4px;border-radius:2px;
 background:linear-gradient(180deg,#b8b8b8,#4a4a4a)}
.pipe:before{left:0}
.pipe:after{right:0}
.pipe.act{background:linear-gradient(180deg,#ffbb77,#d1400f 55%,#6f1f04);
 box-shadow:inset 0 1px 0 rgba(255,255,255,.45),0 0 12px rgba(230,90,20,.65)}

/* ---------- Runde Aktionsschaltflächen ---------- */
.rb{width:124px;background:none;border:none;color:#c4c4c4;font:inherit;font-size:.78rem;cursor:pointer;
 display:flex;flex-direction:column;align-items:center;gap:6px;padding:0;line-height:1.25}
.rb span{width:72px;height:72px;border-radius:50%;border:2px solid #767676;display:flex;align-items:center;
 justify-content:center;font-size:1.8rem;background:radial-gradient(circle at 38% 26%,#5a5a5a,#1c1c1c 72%);
 box-shadow:inset 0 2px 4px rgba(255,255,255,.14),0 4px 10px rgba(0,0,0,.6)}
.rb:hover span{border-color:#b0b0b0}
.rb:hover{color:#fff}
.rb.on span{border-color:var(--y2);background:radial-gradient(circle at 38% 26%,#ffd980,var(--y) 55%,var(--yd));
 box-shadow:inset 0 2px 4px rgba(255,255,255,.5),0 0 14px rgba(245,168,0,.45)}
.rb.on{color:var(--y)}
.rb.dng:hover span{border-color:var(--red);box-shadow:inset 0 2px 4px rgba(255,255,255,.1),0 0 14px rgba(224,58,47,.45)}

/* ---------- Listen und Tabellen ---------- */
.rows{margin-top:12px;border-top:1px solid #3a3a3a}
.rows div{display:flex;justify-content:space-between;gap:10px;padding:7px 4px;border-bottom:1px solid #2c2c2c;font-size:.9rem}
.rows span:first-child{color:var(--tx2)}
.rows span:last-child{color:var(--val);font-weight:700;text-align:right}
.rows span.w{color:var(--y)}
.rows span.e{color:#ff8079}
.cols{display:grid;grid-template-columns:repeat(auto-fit,minmax(250px,1fr));gap:0 22px}
.grp{margin-top:14px}
.grp>h2{font-size:.74rem;text-transform:uppercase;letter-spacing:1px;color:var(--y);font-weight:700;
 border-bottom:1px solid #3a3a3a;padding-bottom:5px;margin-bottom:2px}
table{width:100%;border-collapse:collapse;font-size:.85rem}
th,td{padding:6px 9px;text-align:left;border-bottom:1px solid #2c2c2c}
th{color:var(--y);font-size:.7rem;text-transform:uppercase;letter-spacing:.6px;background:#1a1a1a}
td:nth-child(n+2){text-align:right;color:var(--val);font-variant-numeric:tabular-nums}
td:first-child{color:var(--tx2)}
tr.stale td{opacity:.4}
.tw{overflow-x:auto;border:1px solid #333;border-radius:6px}

/* ---------- Bedienelemente ---------- */
button.a{background:linear-gradient(180deg,#4a4a4a,#2a2a2a);color:var(--tx);border:1px solid #6a6a6a;
 border-radius:6px;padding:9px 15px;font:inherit;font-size:.86rem;cursor:pointer}
button.a:hover{background:linear-gradient(180deg,#5c5c5c,#363636)}
button.a.p{background:linear-gradient(180deg,var(--y2),var(--y));color:#151515;border-color:var(--y2);font-weight:700}
button.a:disabled{opacity:.45;cursor:default}
.btns{display:flex;flex-wrap:wrap;gap:9px;margin-top:11px}
input[type=number],input[type=search]{background:#1c1c1c;border:1px solid #555;border-radius:6px;padding:9px 11px;
 font:inherit;font-size:.88rem;color:var(--tx);width:100%}
input[type=range]{width:100%;accent-color:var(--y)}
label.chip{display:inline-flex;align-items:center;gap:7px;border:1px solid #555;border-radius:6px;padding:8px 11px;
 font-size:.84rem;background:#1c1c1c}
.hint{font-size:.8rem;color:var(--tx2);line-height:1.55;margin-top:8px}
pre.scr{background:#06180a;color:#7dfb5a;font-family:Consolas,monospace;font-size:1rem;line-height:1.45;padding:14px;
 border:1px solid #2c4a2c;border-radius:6px;white-space:pre;overflow-x:auto;min-height:5.2em;margin-top:10px;font-size:.95rem}
pre.scr .ln{display:block;padding:1px 8px;margin:0 -8px;border-radius:4px}
pre.scr .sel{background:#7dfb5a;color:#06180a;font-weight:bold}
.dsp{max-width:340px}
.keys{display:grid;grid-template-columns:repeat(2,1fr);gap:8px;margin-top:11px}
.keys button{min-height:46px}
.keys .ok2{border-color:#3d7a3d}
.keys .warn{border-color:#7a4a2a;color:#ffb27a}
.keys .warn:nth-last-child(2){margin-top:10px}
.keys .warn:last-child{margin-top:10px}
.hide{display:none}
.msg{font-size:.84rem;margin-top:9px;min-height:1.1em;color:var(--tx2)}
.ok{color:var(--grn)}.err{color:#ff8079}

/* ---------- Fußleiste ---------- */
footer{display:flex;align-items:center;gap:16px;background:linear-gradient(180deg,#141414,#000);
 border-top:1px solid #333;padding:8px 14px;font-size:.92rem;position:sticky;bottom:0;z-index:30}
footer>span{display:inline-flex;align-items:center}
footer .tl{border:1px solid var(--y);border-radius:4px;padding:2px 8px;font-size:.95rem;color:var(--y);
 background:#0d0d0d}
footer .dt{margin-left:auto;color:#fff;font-variant-numeric:tabular-nums;white-space:nowrap}
footer .at{color:#fff;font-weight:600}
footer .at i{font-style:normal;color:#e0584a;font-weight:400;margin-right:5px}
.dot{width:9px;height:9px;border-radius:50%;display:inline-block;margin-right:6px}
.dot.on{background:var(--grn);box-shadow:0 0 7px var(--grn)}
.dot.off{background:var(--red);box-shadow:0 0 7px var(--red)}
#toast{position:fixed;left:50%;transform:translateX(-50%);bottom:58px;z-index:60;background:#1e1e1e;
 border:2px solid var(--y);border-radius:8px;padding:10px 18px;font-size:.92rem;color:#fff;display:none;
 box-shadow:0 6px 20px rgba(0,0,0,.7);max-width:92vw}
#toast.err{border-color:var(--red)}

/* ---------- Karten-Raster (PC: mehrspaltig, Handy: einspaltig) ---------- */
.frame>div:not(.hide):not(#t8):not(#t9){display:grid;grid-template-columns:minmax(0,1fr);
 gap:14px;align-items:start}
.frame>div:not(.hide):not(#t8):not(#t9)>.sbar,.frame>div:not(.hide):not(#t8):not(#t9)>.stage,
.frame>div:not(.hide):not(#t8):not(#t9)>.hint,.frame>div:not(.hide):not(#t8):not(#t9)>.btns{grid-column:1/-1;margin:0}
.grp{margin-top:0;background:rgba(0,0,0,.28);border:1px solid #333;border-radius:8px;padding:10px 14px 6px}
.grp .rows{margin-top:6px;border-top:none}
.grp .cols{grid-template-columns:1fr}
.grp .tw{margin:8px 0}
#t8 .grp,#t9 .grp{margin-top:14px}
#nav button{min-height:44px}
button.a,label.chip{min-height:40px}
input[type=number],input[type=search]{font-size:16px}
#fclock .d{display:inline;margin-right:.45em}
.hq{white-space:nowrap}

/* ---------- PC / breite Bildschirme ---------- */
@media(min-width:900px){
 .frame>div:not(.hide):not(#t8):not(#t9){grid-template-columns:repeat(2,minmax(0,1fr))}
}
@media(min-width:1100px){
 main{max-width:1480px;padding:16px}
 .frame{padding:18px}
 .stage{gap:20px}
 .scene{min-height:260px;gap:16px}
 .side{flex:0 0 160px}
 #nav button{padding:14px 20px;font-size:.92rem}
 .sbar{font-size:1.15rem}
}
@media(min-width:1700px){
 .frame>div:not(.hide):not(#t8):not(#t9){grid-template-columns:repeat(3,minmax(0,1fr))}
}

/* ---------- Tablet ---------- */
@media(max-width:900px){
 .stage{flex-direction:column}
 .side{flex:1 1 auto;width:100%;flex-direction:row;flex-wrap:wrap;justify-content:center}
}

/* ---------- Handy ---------- */
@media(max-width:700px){
 body{font-size:15px}
 #hd{padding-top:env(safe-area-inset-top)}
 .logo{padding:0 10px}
 .logo b{font-size:.9rem;padding:1px 5px;letter-spacing:1px}
 #nav{scroll-snap-type:x proximity;-webkit-overflow-scrolling:touch}
 #nav button{scroll-snap-align:center;padding:0 15px;font-size:.92rem;min-height:48px}
 .hq{padding:0 13px;font-size:1.15rem}
 main{padding:0;max-width:none}
 .frame{border:none;border-radius:0;padding:10px 10px 14px;box-shadow:none;
  background:radial-gradient(120% 60% at 50% 0%,#2e2e2e,#0e0e0e 70%,#080808)}
 .sbar{font-size:1rem;padding:10px 14px;margin-bottom:10px;border-radius:7px}
 .stage{gap:10px}
 .scene{display:flex;flex-wrap:wrap;justify-content:center;align-items:flex-end;gap:12px 6px;overflow:visible;
  min-height:0;padding:14px 8px}
 .scene>.pipe{display:none}
 .scene>.gfx{order:1;zoom:.74}
 .scene>.vcol{order:2;flex:1 1 100%;display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
 .scene>.vcol.r{order:3}
 .scene>.vcol .vb{min-width:0;padding:6px 4px;font-size:.98rem}
 .vcol.l .vb:after,.vcol.r .vb:after,.vcol.l .vb:before,.vcol.r .vb:before{display:none}
 .side{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;width:100%}
 .rb{width:auto;font-size:.74rem}
 .rb span{width:58px;height:58px;font-size:1.5rem}
 .grp{padding:8px 12px 4px}
 .frame>div:not(.hide):not(#t8):not(#t9){grid-template-columns:minmax(0,1fr);gap:12px}
 .dsp{max-width:none}
 .keys button{min-height:50px}
 .btns button.a{flex:1 1 auto}
 footer{padding:8px 12px calc(8px + env(safe-area-inset-bottom));gap:10px;font-size:.86rem}
 footer .tl{display:none}
 footer>span{white-space:nowrap;min-width:0}
 #fclock .d{display:none}
 #toast{bottom:calc(62px + env(safe-area-inset-bottom))}
 .tw{-webkit-overflow-scrolling:touch}
 td:first-child{position:sticky;left:0;background:#171717}
 th:first-child{position:sticky;left:0}
}
@media(max-width:380px){
 .scene>.gfx{zoom:.62}
 .scene>.vcol .vb{font-size:.9rem}
 footer{font-size:.8rem;gap:8px}
}
</style>
</head>
<body>

<div id="hd">
 <div class="logo"><b>ETA</b></div>
 <div id="nav"></div>
 <button class="hq" onclick="tab(9)" title="Kesseldisplay">?</button>
</div>

<main>
 <div class="frame">
  <div id="t0"></div>
  <div id="t1" class="hide"></div>
  <div id="t2" class="hide"></div>
  <div id="t3" class="hide"></div>
  <div id="t4" class="hide"></div>
  <div id="t5" class="hide"></div>
  <div id="t6" class="hide"></div>
  <div id="t7" class="hide"></div>

  <div id="t8" class="hide">
   <div class="sbar" id="pgBar">Einstellwerte der Regelung</div>
   <p class="hint">Liest die komplette Parameterliste des Kessels aus (Sollwerte, Heizgrenzen, Urlaub bis, Solarvorrang &hellip;).
    Der Vorgang dauert etwa eine Minute, w&auml;hrenddessen pausiert die Messwerterfassung.</p>
   <div class="btns"><button class="a p" id="pgBtn" onclick="pgStart()">Einstellungen laden</button></div>
   <p id="pgSt" class="msg"></p>
   <div id="pgWrap" class="hide">
    <div style="margin-top:12px"><input type="search" id="pgQ" placeholder="Suchen, z.B. Urlaub, SOLL, heizen&hellip;" oninput="pgRender()"></div>
    <div id="pgOut"></div>
   </div>
  </div>

  <div id="t9" class="hide">
   <div class="btns">
    <button class="a" id="dispBtnOn" onclick="dispMode(1)">Anzeige ein</button>
    <button class="a" id="dispBtnOff" onclick="dispMode(0)">Anzeige aus</button>
    <button class="a" onclick="dispRefresh()">Aktualisieren</button>
    <label class="chip"><input type="checkbox" id="dispAuto" onchange="dispAutoTgl()"> Auto (2 s)</label>
   </div>
   <p id="dispSt" class="msg"></p>
   <div class="dsp">
   <pre class="scr" id="dispText">(Anzeige aus)</pre>
   <div class="keys">
    <button class="a" onclick="dispKey('auf')">&#9650; Auf</button>
    <button class="a" onclick="dispKey('ab')">&#9660; Ab</button>
    <button class="a ok2" onclick="dispKey('ja')">Ja</button>
    <button class="a" onclick="dispKey('zurueck')">Zur&uuml;ck</button>
    <button class="a" onclick="dispKey('aendern')">&Auml;ndern</button>
    <button class="a" onclick="dispKey('info')">Info</button>
    <button class="a warn" onclick="dispKey('kessel')">Kessel Ein/Aus</button>
    <button class="a warn" onclick="dispKey('entaschen')">Entaschen</button>
   </div>
   </div>
   <p class="hint">Solange die Anzeige eingeschaltet ist, ruht die Messwerterfassung und ein OTA-Update ist nicht m&ouml;glich.</p>
  </div>
 </div>
</main>

<div id="toast"></div>

<footer>
 <span class="tl">&#128736;</span>
 <span><span id="dot" class="dot off"></span><span id="fstat">verbinde&hellip;</span></span>
 <span class="at"><i>&#127777;</i><span id="faussen">&ndash;</span></span>
 <span class="dt" id="fclock"></span>
</footer>

<script>
function el(i){return document.getElementById(i);}
function esc(s){return String(s).replace(/[&<>]/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;'}[c];});}

var TABS=['Anlage','Kessel','Puffer','Warmwasser','Heizkreise','Solar','Lager','Sys','Parameter','Display'];
var cur=0;
(function(){var h='';for(var i=0;i<TABS.length;i++)h+='<button onclick="tab('+i+')">'+TABS[i]+'</button>';el('nav').innerHTML=h;})();
function tab(n){
 cur=n;
 var b=el('nav').children;
 for(var i=0;i<b.length;i++){b[i].className=(i===n?'act':'');el('t'+i).className=(i===n?'':'hide');}
 if(n===9)dispRefresh(); else dispAutoStop();
 var nv=el('nav');
 if(b[n])nv.scrollTo({left:b[n].offsetLeft-(nv.clientWidth-b[n].offsetWidth)/2,behavior:'smooth'});
 window.scrollTo(0,0);
 if(lastData)render(lastData);
}

/* ---- Klartexte der Zustandswerte, ausgelesen aus der Textliste des Kessels ---- */
var EA={0:'Aus',1:'Ein'},JN={0:'Nein',1:'Ja'},OKF={0:'St\u00f6rung',1:'OK'},MIX={0:'Zu',1:'Halt',2:'Auf'};
var HKZ={0:'Ein Tag',1:'Ein Nacht',2:'Ein Fern Tag',3:'Ein Fern Nacht',4:'Aus Soll<Raum Tag',5:'Aus Soll<Raum Nacht',
 6:'Aus Soll< Tag',7:'Aus Soll< Nacht',8:'Aus Au\u00dfen>Tag',9:'Aus Au\u00dfen>Nacht',10:'Aus Sommer',
 11:'Aus Kessel zu kalt',12:'Aus Puffer zu kalt',13:'WW-Vorrang',14:'Frostschutz',15:'Ein \u00dcbertemperatur',16:'St\u00f6rung'};
var BETR={0:'Automatik',1:'Aus (Sommer)',2:'Urlaub',3:'Tag',4:'Nacht'};
var PARTY={0:'Automatik',1:'Tag',2:'Nacht'};
var M={
 Betriebszustand:{0:'Ausgeschaltet',1:'Heizversuch',2:'Z\u00fcndet',3:'Heizt',4:'Glutabbrand',5:'Bereit',
  6:'Entaschen',7:'Entaschen',8:'St\u00f6rung',32:'Gesperrt',33:'Fremdw\u00e4rme'},
 Boiler_Status:{0:'Laden',1:'Geladen',2:'Fordert an',3:'Nachlauf',4:'St\u00f6rung'},
 Puffer_Status:{0:'Geladen',1:'Fordert an',2:'St\u00f6rung',3:'\u00dcbertemp. laden'},
 Holzkessel:{0:'T\u00fcr offen',1:'Heizt an',2:'In Betrieb',3:'\u00dcbertemperatur',4:'Glutabbrand',5:'Feuer aus',6:'Ausgeschaltet',7:'St\u00f6rung'},
 Kollektor_Status:{0:'Aus',1:'Ein',2:'Aus Speicher>',3:'St\u00f6rung',4:'Aus Brennertemp.<',5:'Aus Kollektor>'},
 Solar_Ladung_auf:{0:'Puffer',1:'Boiler',2:'Nachladen',3:'Absenkung'},
 Solarvorrang:{0:'Deaktiviert',1:'Au\u00dfen<',2:'Kein',3:'Mittel',4:'Hoch'},
 Lambdasonde:{0:'OK',1:'Heizt auf',2:'Zu kalt',3:'Kurzschluss',4:'Wird gesucht',5:'Unterbrochen',6:'Ausgeschaltet'},
 Raumaustragung:{0:'Aus',1:'F\u00f6rdert',2:'Nachlauf',3:'St\u00f6rung',4:'Vorlauf',5:'Standby'},
 Austragschnecke:{0:'Retour',1:'Halt',2:'Vor'},
 Anfahrentlastung:{0:'Unten',1:'Oben'},
 Kesseltuer:{0:'Offen',1:'Zu'},
 Sicherung_24VAC:{0:'Defekt',1:'OK'},
 Boiler_Extraladung:JN,Saugen:JN,
 Stoerungsmeldung:EA,Entaschung:EA,Thermostat:EA,
 Wassermangel:OKF,STB:OKF,Kesseltemp_Waechter:OKF,RL_tief_Warnung:OKF,
 Rostklappe:MIX,Ruecklaufmischer:MIX,Mischer_MK1:MIX,Mischer_MK2:MIX,Mischer_MK3:MIX,
 MK1:HKZ,MK2:HKZ,MK3:HKZ,
 Betrieb_MK1:BETR,Betrieb_MK2:BETR,Betrieb_MK3:BETR,
 Party_MK1:PARTY,Party_MK2:PARTY,Party_MK3:PARTY
};
/* Werte, die als St\u00f6rung gelten */
var BAD={Betriebszustand:[8],Boiler_Status:[4],Puffer_Status:[2],Holzkessel:[7],Kollektor_Status:[3],
 Wassermangel:[0],STB:[0],Kesseltemp_Waechter:[0],Sicherung_24VAC:[0],
 Lambdasonde:[2,3,5],Raumaustragung:[3],MK1:[16],MK2:[16],MK3:[16]};
var LBL={Wassermangel:'Wassermangel',STB:'Sicherheitstemperaturbegrenzer',Kesseltemp_Waechter:'Kesseltemperaturw\u00e4chter',
 Sicherung_24VAC:'Sicherung 24 V AC',Kesseltuer:'Kesselt\u00fcr',Stoerungsmeldung:'St\u00f6rmelderelais',
 RL_tief_Warnung:'R\u00fccklauf zu tief',Betriebszustand:'Pelletskessel',Boiler_Status:'Warmwasserspeicher',
 Puffer_Status:'Pufferspeicher',Holzkessel:'Holzkessel',Kollektor_Status:'Solarkollektor',Lambdasonde:'Lambdasonde',
 Raumaustragung:'Raumaustragung',MK1:'Heizkreis 1',MK2:'Heizkreis 2',MK3:'Heizkreis 3'};

var S={},lastData=null,t=0,ivPend=null;
function v(n){var x=S[n];return (x&&x.value!==null&&x.value!==undefined)?x.value:null;}
function has(n){return v(n)!==null;}
function unit(n){return S[n]?(S[n].unit||'').replace('\u00c2',''):'';}
function lbl(n){return LBL[n]||n.replace(/_/g,' ');}
function fmt(n,d){
 var x=v(n);if(x===null)return '\u2013';
 var u=unit(n);
 if(d===undefined){d=1;if(u===''||u==='%'||u==='U/min'||u==='h'||Math.abs(x)>=1000)d=0;if(u==='t')d=2;}
 return x.toFixed(d)+(u?' '+u:'');
}
function txt(n){
 var x=v(n);if(x===null)return '';
 var m=M[n];
 if(m){var s=m[Math.round(x)];return s!==undefined?s:'';}
 return fmt(n);
}
function bad(n){
 var x=v(n);if(x===null)return false;
 var b=BAD[n];return !!(b&&b.indexOf(Math.round(x))>=0);
}
/* Speicherfarbe wie am Kesseldisplay: kalt blau, lau violett, ab ~50 Grad rot */
function tcol(c,a){
 if(c===null)return '#4a4a4a';
 var p=Math.max(0,Math.min(1,(c-25)/27));
 return 'hsla('+(240+120*p)+',74%,'+(46+3*p)+'%,'+(a===undefined?1:a)+')';
}

/* ---------- Bausteine im ETAtouch-Stil ---------- */
function sbar(text,state){return '<div class="sbar '+(state||'')+'">'+text+'</div>';}
function statusBar(n,fallback){
 var s=has(n)?txt(n):'';
 if(!s)return sbar(fallback||'Keine Statusmeldung');
 return sbar(s,bad(n)?'err':'');
}
function vb(label,val,u){
 return '<div class="vb">'+val+(u?'<s>'+u+'</s>':'')+(label?'<em>'+label+'</em>':'')+'</div>';
}
function vbn(label,n,d){
 if(!has(n))return '';
 var x=v(n),u=unit(n);
 if(d===undefined){d=1;if(u===''||u==='%'||u==='U/min'||u==='h'||Math.abs(x)>=1000)d=0;if(u==='t')d=2;}
 return vb(label,x.toFixed(d),u);
}
function vcol(side,items){
 var s=items.filter(Boolean).join('');
 return s?'<div class="vcol '+side+'">'+s+'</div>':'';
}
function stage(scene,side){
 return '<div class="stage"><div class="scene">'+scene+'</div>'+(side?'<div class="side">'+side+'</div>':'')+'</div>';
}
function rb(icon,label,onclick,state){
 return '<button class="rb '+(state||'')+'"'+(onclick?' onclick="'+onclick+'"':' disabled')+
  '><span>'+icon+'</span>'+label+'</button>';
}
function row(l,val,c){return '<div><span>'+l+'</span><span'+(c?' class="'+c+'"':'')+'>'+val+'</span></div>';}
function rowIf(n,l){return has(n)?row(l,fmt(n)):'';}
function rowT(n,l){var s=has(n)?txt(n):'';return s?row(l,s,bad(n)?'e':'w'):'';}
function rows(inner,cols){
 if(!inner)return '';
 return '<div class="rows'+(cols?' cols':'')+'">'+inner+'</div>';
}
function grp(title,inner){return inner?'<div class="grp"><h2>'+title+'</h2>'+inner+'</div>':'';}

/* ---------- 0 Anlage ---------- */
function vOver(){
 var al=alarms();
 var h=al.length?sbar('&#9888; '+al.length+' St\u00f6rung'+(al.length>1?'en':''),'err')
   :statusBar('Betriebszustand','Anlage in Betrieb');
 var k=v('Kessel'),pl=v('Pufferladung'),b=v('Boiler');
 var scene=vcol('l',[vbn('Kessel','Kessel'),vbn('Abgas','Abgas'),vbn('R\u00fccklauf','Kessel_Ruecklauf')])+
  '<div class="gfx"><div class="kessel'+(fire()?' on':'')+'"><i></i></div></div>'+
  '<div class="pipe'+(v('Pufferpumpe')>0?' act':'')+'"></div>'+
  battGfx()+
  '<div class="pipe'+(v('Boilerpumpe')>0?' act':'')+'"></div>'+
  tankGfx(b,'Warmwasser')+
  vcol('r',[vb('Ladung',pl!==null?pl.toFixed(0):'\u2013','%'),vbn('Warmwasser','Boiler'),vbn('Au\u00dfen','Aussen')]);
 var side=rb('&#128293;','Kessel ein',"sendCmd('kessel_ein','Kessel einschalten?')",fire()?'on':'')+
  rb('&#128683;','Kessel aus',"sendCmd('kessel_aus','Kessel ausschalten?')",'dng')+
  rb('&#128703;','WW sofort laden',"sendCmd('boiler','Einmalige Boilerladung starten?')",
     v('Boiler_Extraladung')===1?'on':'');
 h+=stage(scene,side);
 var r='';
 for(var i=0;i<al.length;i++)r+=al[i];
 h+=grp('St\u00f6rungen',rows(r));
 h+=grp('Anlage',rows(
   rowT('Betriebszustand','Pelletskessel')+rowT('Holzkessel','Holzkessel')+
   rowIf('Kessel','Kesseltemperatur')+rowIf('Kessel_Soll','Kessel Soll')+
   rowIf('Pufferladung','Pufferladung')+rowT('Puffer_Status','Puffer')+
   rowIf('Boiler','Warmwasser')+rowT('Boiler_Status','Warmwasser Zustand')+
   rowIf('Aussen','Au\u00dfentemperatur')+rowT('Kollektor_Status','Solar'),1));
 var hk='';
 for(var j=1;j<=3;j++){
  if(!has('Vorlauf_MK'+j)&&!has('MK'+j))continue;
  hk+=row('Heizkreis '+j,(has('Vorlauf_MK'+j)?v('Vorlauf_MK'+j).toFixed(0)+' \u00b0C \u00b7 ':'')+
    (txt('MK'+j)||'\u2013'),bad('MK'+j)?'e':'w');
 }
 h+=grp('Heizkreise',rows(hk));
 return h;
}
function fire(){var b=v('Betriebszustand');return b===1||b===2||b===3||b===4;}
function alarms(){
 var out=[];
 for(var k in BAD)if(bad(k)&&txt(k))out.push(row(lbl(k),txt(k),'e'));
 return out;
}

/* ---------- Grafikbausteine ---------- */
function battGfx(){
 var o=v('Puffer_oben'),m=v('Puffer_mitte'),u=v('Puffer_unten'),pl=v('Pufferladung');
 var g='linear-gradient(180deg,'+tcol(o)+' 0%,'+tcol(m)+' 50%,'+tcol(u)+' 100%)';
 var lab='';
 if(o!==null)lab+='<div class="ovl" style="top:16%">'+o.toFixed(0)+'<s>\u00b0C</s></div>';
 if(m!==null)lab+='<div class="ovl" style="top:44%">'+m.toFixed(0)+'<s>\u00b0C</s></div>';
 if(u!==null)lab+='<div class="ovl" style="top:72%">'+u.toFixed(0)+'<s>\u00b0C</s></div>';
 return '<div class="gfx"><div class="batt"><u></u><i style="background:'+g+'"><span class="shine"></span></i>'+
  lab+'</div>'+(pl!==null?'<div class="cap"><b style="color:var(--y2);font-size:.92rem">'+pl.toFixed(0)+
  ' %</b> geladen</div>':'')+'</div>';
}
function tankGfx(temp,cap){
 var g='linear-gradient(180deg,'+tcol(temp)+','+tcol(temp!==null?temp-10:null)+')';
 return '<div class="gfx"><div class="tk"><i style="background:'+g+'"><span class="shine"></span></i>'+
  (temp!==null?'<div class="ovl" style="top:42%">'+temp.toFixed(0)+'<s>\u00b0C</s></div>':'')+'</div>'+
  '<div class="cap">'+cap+'</div></div>';
}

/* ---------- 1 Kessel ---------- */
function vKessel(){
 var h=statusBar('Betriebszustand','Kessel');
 var o2=has('O2_Ist')?'O2_Ist':'Restsauerstoff';
 var scene=vcol('l',[vbn('Kessel','Kessel'),vbn('Abgas','Abgas'),vbn('R\u00fccklauf','Kessel_Ruecklauf'),
   vbn('Rest-O2',o2)])+
  '<div class="gfx"><div class="kessel'+(fire()?' on':'')+'"><i></i></div>'+
  '<div class="cap">Pelletskessel</div></div>'+
  vcol('r',[vbn('Leistung','Leistung_Soll'),vbn('Gebl\u00e4se','Geblaesedrehzahl'),
   vbn('Luftschieber','Luftschieber'),vbn('Kessel Soll','Kessel_Soll')]);
 var side=rb('&#128293;','Einschalten',"sendCmd('kessel_ein','Kessel einschalten?')",fire()?'on':'')+
  rb('&#128683;','Ausschalten',"sendCmd('kessel_aus','Kessel ausschalten?')",'dng');
 h+=stage(scene,side);
 h+=grp('Temperaturen',rows(
   rowIf('Kessel','Kesseltemperatur')+rowIf('Kessel_Soll','Kessel Sollwert')+
   rowIf('Kessel_Ruecklauf','R\u00fccklauf')+rowIf('Ruecklauf_Soll','R\u00fccklauf Sollwert')+
   rowT('Ruecklaufmischer','R\u00fccklaufmischer')+
   rowIf('Abgas','Abgastemperatur')+rowIf('Abgas_Soll','Abgas Sollwert')+
   rowIf('Abgas_Kessel','Abgas Holzkessel')+rowIf('Abgas_Soll_K','Abgas Soll Holzkessel'),1));
 h+=grp('Verbrennung',rows(
   rowIf(o2,'Restsauerstoff')+rowIf('O2_Soll','Restsauerstoff Sollwert')+rowT('Lambdasonde','Lambdasonde')+
   rowIf('Luftschieber','Luftschieber')+rowIf('Luftschieber_Soll','Luftschieber Sollwert')+
   rowIf('Geblaesedrehzahl','Saugzuggebl\u00e4se')+rowIf('Geblaese_Soll','Gebl\u00e4se Sollwert')+
   rowIf('Leistung_Soll','Leistung')+rowIf('Leistung_Soll_K','Anforderung Kessel'),1));
 h+=grp('Holzkessel und Antriebe',rows(
   rowT('Holzkessel','Holzkessel')+rowT('Anfahrentlastung','Anfahrentlastung')+
   rowT('Austragschnecke','Austragschnecke')+rowT('Raumaustragung','Raumaustragung')+
   rowT('Saugen','Saugen')+rowT('Entaschung','Entaschung')+rowT('Rostklappe','Rostklappe'),1));
 return h;
}

/* ---------- 2 Puffer ---------- */
function vPuffer(){
 var pl=v('Pufferladung');
 var h=statusBar('Puffer_Status',pl!==null?'Pufferspeicher \u00b7 '+pl.toFixed(0)+' % geladen':'Pufferspeicher');
 var scene=vcol('l',[vbn('Oben','Puffer_oben'),vbn('Mitte','Puffer_mitte'),vbn('Unten','Puffer_unten')])+
  battGfx()+
  vcol('r',[vbn('Ladezustand','Pufferladung'),vbn('Soll','Puffer_Soll'),vbn('Pumpe','Pufferpumpe'),
   vbn('Solar','Puffer_Solar')]);
 var side=rb('&#128293;','Puffer sofort laden',"sendCmd('kessel_ein','Kessel einschalten und Puffer laden?')",
   fire()?'on':'');
 h+=stage(scene,side);
 h+=grp('Pufferspeicher',rows(
   rowIf('Puffer_oben','F\u00fchler oben')+rowIf('Puffer_mitte','F\u00fchler mitte')+
   rowIf('Puffer_unten','F\u00fchler unten')+rowIf('Pufferladung','Ladezustand')+
   rowIf('Puffer_Soll','Sollwert')+rowIf('Pufferpumpe','Pufferpumpe')+
   rowIf('Puffer_Solar','F\u00fchler Solarmodul')+rowT('Anfahrentlastung','Anfahrentlastung')+
   rowT('Puffer_Status','Zustand'),1));
 return h;
}

/* ---------- 3 Warmwasser ---------- */
function vWW(){
 var h=statusBar('Boiler_Status','Warmwasserspeicher');
 var b=v('Boiler');
 var scene=vcol('l',[vbn('Ist','Boiler'),vbn('Soll','Boiler_Soll')])+
  tankGfx(b,'Warmwasserspeicher')+
  vcol('r',[vbn('Ladepumpe','Boilerpumpe'),vbn('Solar oben','Boiler_oben_Solar'),
   vbn('Solar unten','Boiler_unten_Solar')]);
 var side=rb('&#128703;','Warmwasser sofort laden',"sendCmd('boiler','Einmalige Boilerladung starten?')",
   v('Boiler_Extraladung')===1?'on':'');
 h+=stage(scene,side);
 h+=grp('Warmwasser',rows(
   rowIf('Boiler','Speichertemperatur')+rowIf('Boiler_Soll','Sollwert')+
   rowIf('Boilerpumpe','Ladepumpe')+rowT('Boiler_Extraladung','Extra laden aktiv')+
   rowT('Boiler_Status','Zustand')+rowIf('Boiler_oben_Solar','Solarf\u00fchler oben')+
   rowIf('Boiler_unten_Solar','Solarf\u00fchler unten'),1));
 h+='<p class="hint">Voraussetzung f\u00fcr das Schalten: am Kessel muss <b>SMS erlauben = JA</b> eingestellt sein.</p>';
 return h;
}

/* ---------- 4 Heizkreise ---------- */
function vHK(){
 var list=[];
 for(var i=1;i<=3;i++)if(has('Vorlauf_MK'+i)||has('MK'+i))list.push(i);
 if(!list.length)return sbar('Keine Heizkreisdaten empfangen','off');
 var h=statusBar('MK'+list[0],'Heizkreise');
 var scene='';
 for(var j=0;j<list.length;j++){
  var n=list[j],pu=v('Pumpe_MK'+n);
  scene+='<div class="gfx" style="text-align:center">'+
   '<div class="rad" style="opacity:'+(pu>0?1:.4)+'"></div>'+
   '<div style="margin-top:9px"><div class="ovl st">'+
   (has('Vorlauf_MK'+n)?v('Vorlauf_MK'+n).toFixed(0):'\u2013')+'<s>\u00b0C</s></div></div>'+
   '<div class="cap">Heizkreis '+n+
   (has('Raum_MK'+n)?' \u00b7 Raum '+v('Raum_MK'+n).toFixed(1)+' \u00b0C':'')+'</div></div>';
 }
 var bm=v('Betrieb_MK1');
 var side=rb('&#9728;','Heizen',"sendCmd('tag','Alle Heizkreise auf TAG?')",bm===3?'on':'')+
  rb('&#127769;','Absenken',"sendCmd('nacht','Alle Heizkreise auf NACHT?')",bm===4?'on':'')+
  rb('&#128337;','Zeitautomatik',"sendCmd('auto','Alle Heizkreise auf AUTO?')",bm===0?'on':'');
 h+=stage(scene,side);
 for(var k=0;k<list.length;k++){
  var m=list[k];
  h+=grp('Heizkreis '+m,rows(
    rowIf('Vorlauf_MK'+m,'Vorlauftemperatur')+rowIf('Vorlauf_Soll_MK'+m,'Vorlauf Sollwert')+
    rowIf('Raum_MK'+m,'Raumtemperatur')+rowT('MK'+m,'Zustand')+
    rowT('Betrieb_MK'+m,'Betriebsart')+rowT('Party_MK'+m,'Fernbedienung')+
    rowIf('Pumpe_MK'+m,'Heizkreispumpe')+rowT('Mischer_MK'+m,'Mischer')+
    rowIf('Freigabe_MK'+m,'Freigabetemperatur'),1));
 }
 h+=grp('Witterung',rows(rowIf('Aussen','Au\u00dfentemperatur')));
 h+='<p class="hint">Die Heiz- und Ladezeiten je Wochentag lassen sich \u00fcber die serielle Schnittstelle nicht auslesen. '+
  'Sie sind nur am Kesseldisplay einstellbar (Register <b>Display</b>).</p>';
 return h;
}

/* ---------- 5 Solar ---------- */
function vSolar(){
 if(!has('Kollektor'))return sbar('Keine Solardaten empfangen','off');
 var h=statusBar('Kollektor_Status','Solaranlage');
 var kp=v('Kollektorpumpe');
 var scene=vcol('l',[vbn('Kollektor','Kollektor'),vbn('Leistung','Solarleistung')])+
  '<div class="gfx" style="text-align:center"><div class="coll" style="opacity:'+(kp>0?1:.5)+'"></div>'+
  '<div class="cap">Kollektor</div></div>'+
  '<div class="pipe'+(kp>0?' act':'')+'"></div>'+
  vcol('r',[vbn('Pumpe','Kollektorpumpe'),vbn('Puffer','Puffer_Solar'),vbn('Boiler','Boiler_oben_Solar')]);
 h+=stage(scene,'');
 h+=grp('Solaranlage',rows(
   rowIf('Kollektor','Kollektortemperatur')+rowT('Kollektor_Status','Zustand')+
   rowIf('Kollektorpumpe','Kollektorpumpe')+rowIf('Solarleistung','Solarleistung')+
   rowT('Solar_Ladung_auf','Ladung auf')+rowT('Solarvorrang','Solarvorrang')+
   rowIf('Puffer_Solar','Puffer Solarf\u00fchler')+rowIf('Boiler_oben_Solar','Boiler oben')+
   rowIf('Boiler_unten_Solar','Boiler unten'),1));
 h+=grp('Thermostatfunktion',rows(
   rowT('Thermostat','Zustand')+rowIf('Thermostat_heiss','Hei\u00dfe Seite')+rowIf('Thermostat_kalt','Kalte Seite'),1));
 return h;
}

/* ---------- 6 Lager ---------- */
function vLager(){
 var pv=v('Pelletsvorrat'),cap=parseFloat(localStorage.getItem('siloT')||'6');
 var pct=(pv!==null&&cap>0)?Math.max(0,Math.min(100,pv/cap*100)):0;
 var col=pct<15?'linear-gradient(180deg,#e06a4a,#a03018)':'linear-gradient(180deg,#d09a4a,#8a5a1c)';
 var h=sbar('Pelletslager \u00b7 '+(pv!==null?pv.toFixed(2)+' t':'kein Wert')+' \u00b7 '+pct.toFixed(0)+' %',
   pct<15?'err':'');
 var scene=vcol('l',[vbn('Vorrat','Pelletsvorrat'),vbn('Beh\u00e4lter','Behaelterinhalt')])+
  '<div class="gfx"><div class="silo"><i style="height:'+pct+'%;background:'+col+'"></i>'+
  '<div class="ovl" style="top:8px">'+pct.toFixed(0)+'<s>%</s></div></div>'+
  '<div class="cap">Lagerraum</div></div>'+
  vcol('r',[vbn('Gesamt','Gesamtverbrauch'),vbn('Seit Wartung','Wartung_Verbrauch')]);
 h+=stage(scene,'');
 h+=grp('Vorrat',rows(
   rowIf('Pelletsvorrat','Lagerbestand')+rowIf('Behaelterinhalt','Beh\u00e4lter am Kessel')+
   rowIf('Gesamtverbrauch','Gesamtverbrauch')+rowIf('Wartung_Verbrauch','Seit letzter Wartung')+
   rowT('Raumaustragung','Raumaustragung')+rowT('Saugen','Saugen'),1));
 h+=grp('Entaschung',rows(
   rowT('Entaschung','Zustand')+rowIf('kg_seit_Entasch','kg seit letzter Entaschung')+
   rowIf('Verbr_seit_Entasch','kg seit Aschebox leeren'),1));
 h+=grp('Betriebszeiten',rows(
   rowIf('Betriebsstunden','Betriebsstunden')+rowIf('Abgasgeblaese_Lz','Abgasgebl\u00e4se')+
   rowIf('Wartung_Kessel','Seit letzter Wartung')+rowIf('Uebertemp_Zaehler','\u00dcbertemperaturen'),1));
 h+='<div class="btns"><input type="number" id="siloT" step="0.5" min="0.5" style="max-width:130px" value="'+cap+'">'+
  '<button class="a" onclick="saveSilo()">Lagergr\u00f6\u00dfe speichern</button></div>';
 return h;
}

/* ---------- 7 Sys ---------- */
function vSys(){
 var d=lastData||{};
 var h=sbar('System \u00b7 Firmware '+esc(d.fw||'?')+' \u00b7 '+(d.online?'verbunden':'keine Verbindung'),
   d.online?'':'err');
 var sk=['Wassermangel','STB','Kesseltemp_Waechter','Sicherung_24VAC','Kesseltuer','Stoerungsmeldung','RL_tief_Warnung'];
 var rr='';
 for(var i=0;i<sk.length;i++)rr+=rowT(sk[i],lbl(sk[i]));
 h+=grp('Sicherheitskette',rows(rr,1));
 h+=grp('Verbindung',rows(
   row('Firmware',esc(d.fw||'\u2013'))+row('Laufzeit',fmtDur(d.uptime_s))+
   row('Letzte Daten',(d.last_data_s!==undefined?d.last_data_s+' s':'\u2013'))+
   row('Pakete',d.packets!==undefined?d.packets:'\u2013')+
   row('Timeouts',d.timeouts!==undefined?d.timeouts:'\u2013')+
   row('Messblock',(d.block!==undefined?d.block+' / '+d.blocks:'\u2013'))+
   row('Knoten',d.nodes?Object.keys(d.nodes).join(', '):'\u2013')+
   (d.error?row('Fehler',esc(d.error),'e'):''),1));
 h+=grp('Platinentemperaturen',rows(
   rowIf('Boardtemp','Pelletsteuerung')+rowIf('Boardtemp_max','Maximum Pelletsteuerung')+
   rowIf('Boardtemp_Kessel','Kesselsteuerung')+rowIf('Boardtemp_max_K','Maximum Kesselsteuerung')+
   rowIf('Board_ueber_60','\u00fcber 60 \u00b0C')+rowIf('Board_ueber_70','\u00fcber 70 \u00b0C'),1));
 var iv=(ivPend!==null)?ivPend:(d.interval||60);
 h+=grp('Abfrageintervall',
   '<input type="range" id="ivSlider" min="10" max="600" step="5" value="'+iv+'" '+
   'oninput="ivPend=+this.value;el(\'ivVal\').textContent=this.value+\' s\'">'+
   '<div class="btns" style="align-items:center"><b id="ivVal" style="color:var(--y)">'+iv+' s</b>'+
   '<button class="a p" onclick="setIv()">Speichern</button>'+
   '<button class="a" onclick="sendCmd(\'reset\',\'Fernbedienung zur\\u00fccksetzen?\')">Fernbedienung Reset</button></div>');
 var keys=Object.keys(S).sort(),tr='';
 for(var j=0;j<keys.length;j++){
  var k=keys[j],s=S[k];
  tr+='<tr class="'+(s.stale?'stale':'')+'"><td>'+esc(lbl(k))+'</td><td>'+((M[k]&&txt(k))?txt(k):fmt(k))+
   '</td><td>'+(s.age_s!==undefined?s.age_s+' s':'')+'</td></tr>';
 }
 h+=grp('Alle Messwerte ('+keys.length+')','<div class="tw"><table><thead><tr><th>Bezeichnung</th><th>Wert</th>'+
   '<th>Alter</th></tr></thead><tbody>'+tr+'</tbody></table></div>');
 return h;
}
function fmtDur(s){
 if(s===undefined||s===null)return '\u2013';
 var d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);
 return (d?d+' d ':'')+((h||d)?h+' h ':'')+m+' min';
}

/* ---------- Anzeige aktualisieren ---------- */
var views=[vOver,vKessel,vPuffer,vWW,vHK,vSolar,vLager,vSys];
function render(d){
 lastData=d;S=d.sensors||{};
 el('dot').className='dot '+(d.online?'on':'off');
 el('fstat').textContent=d.online?('Block '+(d.block||1)+'/'+(d.blocks||1)+' \u00b7 '+
   (txt('Betriebszustand')||'\u2013')):'Keine Verbindung';
 el('faussen').textContent=has('Aussen')?v('Aussen').toFixed(1)+' \u00b0C':'\u2013';
 if(cur<views.length)el('t'+cur).innerHTML=views[cur]();
}
function saveSilo(){
 var x=parseFloat(el('siloT').value);
 if(x>0){localStorage.setItem('siloT',x);toast('Lagergr\u00f6\u00dfe gespeichert',1);render(lastData);}
}
function refresh(){
 fetch('/api').then(function(r){return r.json();}).then(render)
  .catch(function(){el('dot').className='dot off';el('fstat').textContent='Keine Verbindung';});
 t=30;
}
var toastTimer=null;
function toast(msg,ok){
 var e=el('toast');
 e.textContent=(ok?'\u2713 ':'\u2717 ')+msg;
 e.className=ok?'':'err';
 e.style.display='block';
 if(toastTimer)clearTimeout(toastTimer);
 toastTimer=setTimeout(function(){e.style.display='none';},4500);
}

/* ---------- Befehle ---------- */
function sendCmd(cmd,q){
 if(!confirm(q))return;
 toast('Sende Befehl\u2026',1);
 fetch('/cmd?cmd='+cmd,{method:'POST',headers:{'X-ETA':'1'}}).then(function(r){return r.json();}).then(function(d){
  toast(d.ok?d.label:d.error,d.ok?1:0);
  if(d.ok)setTimeout(refresh,1500);
 }).catch(function(){toast('Befehl fehlgeschlagen',0);});
}
function setIv(){
 var x=el('ivSlider').value;
 fetch('/interval?val='+x,{method:'POST',headers:{'X-ETA':'1'}}).then(function(r){return r.json();}).then(function(d){
  ivPend=null;toast('Intervall '+d.interval+' s',1);refresh();
 }).catch(function(){toast('Fehler beim Speichern',0);});
}

/* ---------- Parameter ---------- */
var PG=[],pgTimer=null;
function pgGroup(r){
 if(r.k===33){
  if(r.i>=143&&r.i<=151)return 'Solaranlage';
  if(r.i<19)return 'Heizkreis 1';
  if(r.i<38)return 'Heizkreis 2';
  if(r.i<57)return 'Heizkreis 3';
  return 'Heizkreismodul allgemein';
 }
 if(r.k===8)return 'Kessel, Puffer und Boiler';
 if(r.k===16)return 'Pelletsteuerung';
 return 'Knoten '+r.k;
}
function pgDate(x){
 if(!x)return '\u2013';
 var d=x&31,m=(x>>5)&15,y=(x>>9)&127;
 if(d<1||d>31||m<1||m>12)return String(x);
 return ('0'+d).slice(-2)+'.'+('0'+m).slice(-2)+'.'+(2000+y);
}
function pgTime(x){return ('0'+Math.floor(x/60)).slice(-2)+':'+('0'+(x%60)).slice(-2);}
function pgFmt(raw,r){
 if(/urlaub/i.test(r.n))return pgDate(raw);
 if(r.max===1439&&r.min===0)return pgTime(raw);
 var t=r.t||1;
 if(t>1){var dz=String(t).length-1;return (raw/t).toFixed(dz);}
 return String(raw);
}
function pgStart(){
 var b=el('pgBtn'),s=el('pgSt');
 b.disabled=true;s.className='msg';s.textContent='Starte Parameterabfrage\u2026';
 fetch('/params?start=1&level=1',{method:'POST',headers:{'X-ETA':'1'}}).then(function(r){
  if(!r.ok)throw 0;
  s.textContent='Lese Parameterliste\u2026 (etwa eine Minute)';
  pgTimer=setInterval(pgPoll,3000);
 }).catch(function(){b.disabled=false;s.textContent='Start fehlgeschlagen';s.className='msg err';});
}
function pgPoll(){
 fetch('/params?q=%1F').then(function(r){return r.json();}).then(function(d){
  var s=el('pgSt');
  s.textContent='Lese\u2026 '+(d.count||0)+' Parameter';
  if(!d.active&&d.done){
   clearInterval(pgTimer);pgTimer=null;
   fetch('/params').then(function(r){return r.json();}).then(function(f){
    PG=f.rows||[];
    el('pgBtn').disabled=false;
    s.textContent=PG.length+' Parameter gelesen';s.className='msg ok';
    el('pgWrap').className='';pgRender();
   });
  }
 }).catch(function(){});
}
function pgRender(){
 var q=(el('pgQ').value||'').toLowerCase(),g={},order=[];
 for(var i=0;i<PG.length;i++){
  var r=PG[i];
  if(q&&r.n.toLowerCase().indexOf(q)<0)continue;
  var k=pgGroup(r);
  if(!g[k]){g[k]=[];order.push(k);}
  g[k].push(r);
 }
 order.sort();
 var h='';
 for(var j=0;j<order.length;j++){
  var name=order[j],tr='';
  for(var m=0;m<g[name].length;m++){
   var r2=g[name][m];
   tr+='<tr><td>'+esc(r2.n.trim())+'</td><td><b>'+pgFmt(r2.akt,r2)+'</b></td><td>'+
    pgFmt(r2.min,r2)+'</td><td>'+pgFmt(r2.max,r2)+'</td><td>'+pgFmt(r2.std,r2)+'</td></tr>';
  }
  h+=grp(esc(name)+' ('+g[name].length+')','<div class="tw"><table><thead><tr><th>Parameter</th><th>Aktuell</th>'+
   '<th>Min</th><th>Max</th><th>Werk</th></tr></thead><tbody>'+tr+'</tbody></table></div>');
 }
 el('pgOut').innerHTML=h||'<p class="hint">Keine Treffer.</p>';
}

/* ---------- Kesseldisplay ---------- */
var dispTimer=null;
function dispMark(on){
 el('dispBtnOn').className='a'+(on?' p':'');
 el('dispBtnOff').className='a'+(on?'':' p');
}
function dispShow(d){
 var scr=el('dispText');
 dispMark(!!d.on);
 if(d.valid&&d.lines&&d.lines.length){
  // Ein ">" am Zeilenende (auch "<>") markiert die gewaehlte Zeile
  var h='';
  for(var i=0;i<d.lines.length;i++){
   var l=String(d.lines[i]),sel=/\s*(<>|>)\s*$/.test(l);
   if(sel)l=l.replace(/\s*(<>|>)\s*$/,'');
   h+='<span class="ln'+(sel?' sel':'')+'">'+(esc(l)||'&nbsp;')+'</span>';
  }
  scr.innerHTML=h;
 }else scr.textContent=d.on?'(warte auf Daten\u2026)':'(Anzeige aus)';
 el('dispAuto').checked=(dispTimer!==null);
}
function dispRefresh(){
 var s=el('dispSt');
 fetch('/display').then(function(r){return r.json();}).then(function(d){
  dispShow(d);
  if(!d.on){s.textContent='Anzeige aus';s.className='msg';return;}
  s.textContent=d.valid?('Empfangen vor '+d.age_ms+' ms'):'Kein Signal';
  s.className='msg '+(d.valid?'ok':'err');
 }).catch(function(){s.textContent='Fehler';s.className='msg err';});
}
function dispMode(on){
 var s=el('dispSt');
 s.textContent=on?'Schalte Anzeige ein\u2026':'Schalte Anzeige aus\u2026';s.className='msg';
 fetch('/display?on='+on,{method:'POST'}).then(function(r){return r.json();}).then(function(d){
  if(!on){dispAutoStop();el('dispText').textContent='(Anzeige aus)';dispMark(false);
   s.textContent='Anzeige aus';s.className='msg ok';return;}
  dispMark(true);
  s.textContent=d.valid?'Anzeige ein':'Anzeige ein, noch keine Daten';
  s.className='msg '+(d.valid?'ok':'');
  dispRefresh();
 }).catch(function(){s.textContent='Fehler';s.className='msg err';});
}
function dispKey(k){
 // Tasten mit Wirkung auf den Betrieb nur nach Rueckfrage
 if(k==='kessel'&&!confirm('Kessel Ein/Aus wirklich senden?'))return;
 if(k==='entaschen'&&!confirm('Entaschen wirklich senden?'))return;
 fetch('/display?key='+k,{method:'POST',headers:{'X-ETA':'1'}}).then(function(r){
  if(r.status===401){toast('Anmeldung n\u00f6tig \u2013 Seite neu laden und anmelden',0);throw 0;}
  return r.json();}).then(function(d){
  if(d.ok)setTimeout(dispRefresh,300);
  else toast(d.error,0);
 }).catch(function(){toast('Tastendruck fehlgeschlagen',0);});
}
function dispAutoStop(){if(dispTimer){clearInterval(dispTimer);dispTimer=null;}var c=el('dispAuto');if(c)c.checked=false;}
function dispAutoTgl(){
 if(el('dispAuto').checked){dispTimer=setInterval(dispRefresh,2000);dispRefresh();}
 else dispAutoStop();
}

/* ---------- Uhr und Takt ---------- */
var WD=['So.','Mo.','Di.','Mi.','Do.','Fr.','Sa.'];
function clock(){
 var d=new Date(),p=function(x){return ('0'+x).slice(-2);};
 el('fclock').innerHTML='<span class="d">'+WD[d.getDay()]+', '+p(d.getDate())+'.'+p(d.getMonth()+1)+'.'+d.getFullYear()+
  ' </span>'+p(d.getHours())+':'+p(d.getMinutes())+':'+p(d.getSeconds());
}
tab(0);
refresh();
clock();
setInterval(function(){clock();t--;if(t<=0)refresh();},1000);
</script>
</body>
</html>)HTMLEND";
