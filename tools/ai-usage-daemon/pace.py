"""Linear monthly pace — where usage should be if spent evenly through the period."""

from __future__ import annotations

import calendar
from datetime import datetime


def calendar_month_pace_pct(now: datetime | None = None) -> float:
    """Fraction of the current calendar month elapsed (0..100)."""
    now = now or datetime.now()
    days = calendar.monthrange(now.year, now.month)[1]
    elapsed_days = (now.day - 1) + now.hour / 24.0 + now.minute / 1440.0
    return max(0.0, min(100.0, elapsed_days / days * 100.0))


def pace_to_reset(reset_at: int | float, now: datetime | None = None) -> float | None:
    """Pace through a billing period that ends at reset_at (month start → reset day)."""
    now = now or datetime.now()
    try:
        end = datetime.fromtimestamp(int(float(reset_at)))
    except (OSError, ValueError, OverflowError, TypeError):
        return None
    start = end.replace(day=1, hour=0, minute=0, second=0, microsecond=0)
    if now <= start or end <= start:
        return calendar_month_pace_pct(now)
    total = (end - start).total_seconds()
    elapsed = (now - start).total_seconds()
    if total <= 0:
        return None
    return max(0.0, min(100.0, elapsed / total * 100.0))


def attach_pace(block: dict) -> dict:
    """Add pace_pct when the provider has a monthly used/limit quota."""
    out = dict(block)
    if not out.get("ok"):
        return out

    pace: float | None = None
    reset_at = out.get("reset_at")
    limit = out.get("limit")
    label = str(out.get("label") or "").lower()
    unit = str(out.get("unit") or "").lower()

    # Codex monthly credits (billing period ends on reset_at).
    if reset_at is not None and limit:
        pace = pace_to_reset(reset_at)
    # Cursor included requests — monthly cycle (calendar month proxy).
    elif limit and out.get("used") is not None and label not in ("5h", "cycle"):
        if unit in ("credits", "usd", "") or "month" in label:
            pace = pace_to_reset(reset_at) if reset_at else calendar_month_pace_pct()
        elif out.get("used") is not None:
            pace = calendar_month_pace_pct()

    if pace is not None:
        out["pace_pct"] = round(pace, 1)
    return out
