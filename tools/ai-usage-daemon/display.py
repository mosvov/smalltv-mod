"""Shape provider line/sub fields for the SmallTV display."""

from __future__ import annotations


def _fmt_compact(n: float) -> str:
    if n >= 1_000_000:
        text = f"{n / 1_000_000:.1f}M"
    elif n >= 1000:
        text = f"{n / 1000:.1f}K"
    else:
        return str(int(n))
    return text.replace(".0M", "M").replace(".0K", "K")


def _shape_claude(data: dict, mode: str) -> dict:
    out = dict(data)
    mode = (mode or "auto").lower()

    if mode in ("auto", "tokens"):
        if out.get("mode") == "local" and "tokens" in out:
            out["line"] = _fmt_compact(float(out["tokens"]))
            days = out.get("period_days", 30)
            cache = float(out.get("cache_read") or 0)
            if cache > 0 and mode == "auto":
                out["sub"] = f"{_fmt_compact(cache)} cached"
            else:
                out["sub"] = f"{days}d"
        return out

    if mode == "cached" and out.get("mode") == "local":
        cache = float(out.get("cache_read") or 0)
        if cache > 0:
            out["line"] = _fmt_compact(cache)
            out["sub"] = "cached"
        return out

    if mode == "pct" and "s" in out:
        pct = float(out.get("s") or out.get("pct") or 0)
        out["line"] = f"{pct:.0f}%"
        out["sub"] = "5h"
        out["pct"] = pct
        return out

    return out


def _shape_cursor(data: dict, mode: str) -> dict:
    out = dict(data)
    mode = (mode or "auto").lower()
    used = int(out.get("used") or 0)
    limit = int(out.get("limit") or 0)
    pct = float(out.get("pct") or 0)

    if mode == "pct":
        out["line"] = f"{pct:.0f}%"
        out["sub"] = "requests"
        return out

    if mode in ("auto", "requests") and limit > 0:
        if limit >= 1000:
            out["line"] = f"{used}/{limit // 1000}k"
        else:
            out["line"] = f"{used}/{limit}"
        out["sub"] = "requests"
    return out


def _shape_codex(data: dict, mode: str) -> dict:
    out = dict(data)
    mode = (mode or "auto").lower()
    used = int(float(out.get("used") or 0))
    limit = int(float(out.get("limit") or 0))
    remaining = out.get("remaining_pct")
    reset = out.get("reset_label") or ""

    if mode == "remaining" and remaining is not None:
        out["line"] = f"{float(remaining):.0f}% left"
        out["sub"] = f"r {reset}" if reset else "monthly"
        out["pct"] = 100.0 - float(remaining)
        return out

    if mode in ("auto", "credits") and limit > 0:
        if limit >= 1000:
            out["line"] = f"{used}/{limit // 1000}k"
        else:
            out["line"] = f"{used}/{limit}"
        out["sub"] = f"r {reset}" if reset else "credits"
    return out


def apply_display(payload: dict, cfg: dict) -> dict:
    disp = cfg.get("display") or {}
    out = dict(payload)

    for key, shaper, default in (
        ("claude", _shape_claude, "auto"),
        ("cursor", _shape_cursor, "auto"),
        ("codex", _shape_codex, "auto"),
    ):
        block = out.get(key)
        if isinstance(block, dict) and block.get("ok"):
            out[key] = shaper(block, disp.get(key, default))

    return out
