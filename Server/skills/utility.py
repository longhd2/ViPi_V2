from datetime import datetime
from math import floor, pi, sin

from .base import Skill, SkillResult


def _jd_from_date(dd, mm, yy):
    a = floor((14 - mm) / 12)
    y = yy + 4800 - a
    m = mm + 12 * a - 3

    jd = (
        dd
        + floor((153 * m + 2) / 5)
        + 365 * y
        + floor(y / 4)
        - floor(y / 100)
        + floor(y / 400)
        - 32045
    )

    if jd < 2299161:
        jd = (
            dd
            + floor((153 * m + 2) / 5)
            + 365 * y
            + floor(y / 4)
            - 32083
        )

    return jd


def _new_moon(k):
    t = k / 1236.85
    t2 = t * t
    t3 = t2 * t
    dr = pi / 180

    jd1 = (
        2415020.75933
        + 29.53058868 * k
        + 0.0001178 * t2
        - 0.000000155 * t3
    )

    jd1 += (
        0.00033
        * sin(
            (166.56 + 132.87 * t - 0.009173 * t2)
            * dr
        )
    )

    m = (
        359.2242
        + 29.10535608 * k
        - 0.0000333 * t2
        - 0.00000347 * t3
    )

    mpr = (
        306.0253
        + 385.81691806 * k
        + 0.0107306 * t2
        + 0.00001236 * t3
    )

    f = (
        21.2964
        + 390.67050646 * k
        - 0.0016528 * t2
        - 0.00000239 * t3
    )

    c1 = (
        (0.1734 - 0.000393 * t) * sin(m * dr)
        + 0.0021 * sin(2 * m * dr)
        - 0.4068 * sin(mpr * dr)
        + 0.0161 * sin(2 * mpr * dr)
        - 0.0004 * sin(3 * mpr * dr)
        + 0.0104 * sin(2 * f * dr)
        - 0.0051 * sin((m + mpr) * dr)
        - 0.0074 * sin((m - mpr) * dr)
        + 0.0004 * sin((2 * f + m) * dr)
        - 0.0004 * sin((2 * f - m) * dr)
        - 0.0006 * sin((2 * f + mpr) * dr)
        + 0.0010 * sin((2 * f - mpr) * dr)
        + 0.0005 * sin((2 * mpr + m) * dr)
    )

    if t < -11:
        delta_t = (
            0.001
            + 0.000839 * t
            + 0.0002261 * t2
            - 0.00000845 * t3
            - 0.000000081 * t * t3
        )
    else:
        delta_t = (
            -0.000278
            + 0.000265 * t
            + 0.000262 * t2
        )

    return jd1 + c1 - delta_t


def _sun_longitude(jdn):
    t = (jdn - 2451545.0) / 36525
    t2 = t * t
    dr = pi / 180

    m = (
        357.52910
        + 35999.05030 * t
        - 0.0001559 * t2
        - 0.00000048 * t * t2
    )

    l0 = (
        280.46645
        + 36000.76983 * t
        + 0.0003032 * t2
    )

    dl = (
        (1.914600 - 0.004817 * t - 0.000014 * t2)
        * sin(dr * m)
        + (0.019993 - 0.000101 * t)
        * sin(2 * dr * m)
        + 0.000290
        * sin(3 * dr * m)
    )

    l = (l0 + dl) * dr
    l -= pi * 2 * floor(l / (pi * 2))

    return l


def _new_moon_day(k, tz=7):
    return floor(
        _new_moon(k)
        + 0.5
        + tz / 24
    )


def _sun_longitude_sector(jdn, tz=7):
    return floor(
        _sun_longitude(
            jdn - 0.5 - tz / 24
        )
        / pi
        * 6
    )


def _lunar_month_11(yy, tz=7):
    off = _jd_from_date(31, 12, yy) - 2415021
    k = floor(off / 29.530588853)

    nm = _new_moon_day(k, tz)
    sun_long = _sun_longitude_sector(nm, tz)

    if sun_long >= 9:
        nm = _new_moon_day(k - 1, tz)

    return nm


def _leap_month_offset(a11, tz=7):
    k = floor(
        0.5
        + (a11 - 2415021.076998695)
        / 29.530588853
    )

    last = 0
    i = 1

    arc = _sun_longitude_sector(
        _new_moon_day(k + i, tz),
        tz
    )

    while True:
        last = arc
        i += 1

        arc = _sun_longitude_sector(
            _new_moon_day(k + i, tz),
            tz
        )

        if arc == last or i >= 14:
            break

    return i - 1


def solar_to_lunar(dd, mm, yy, tz=7):
    day_number = _jd_from_date(dd, mm, yy)

    k = floor(
        (day_number - 2415021.076998695)
        / 29.530588853
    )

    month_start = _new_moon_day(k + 1, tz)

    if month_start > day_number:
        month_start = _new_moon_day(k, tz)

    a11 = _lunar_month_11(yy, tz)
    b11 = a11

    if a11 >= month_start:
        lunar_year = yy
        a11 = _lunar_month_11(yy - 1, tz)
    else:
        lunar_year = yy + 1
        b11 = _lunar_month_11(yy + 1, tz)

    lunar_day = day_number - month_start + 1

    diff = floor(
        (month_start - a11)
        / 29
    )

    lunar_leap = 0
    lunar_month = diff + 11

    if b11 - a11 > 365:
        leap_diff = _leap_month_offset(a11, tz)

        if diff >= leap_diff:
            lunar_month = diff + 10

            if diff == leap_diff:
                lunar_leap = 1

    if lunar_month > 12:
        lunar_month -= 12

    if lunar_month >= 11 and diff < 4:
        lunar_year -= 1

    return (
        lunar_day,
        lunar_month,
        lunar_year,
        lunar_leap,
    )


class UtilitySkill(Skill):
    name = "utility"
    priority = 20

    async def match(self, intent, ctx):
        return False

    async def handle(self, intent, ctx):
        now = datetime.now()

        if intent.name == "utility.time":
            return SkillResult(
                True,
                now.strftime(
                    "Bây giờ là %H giờ %M phút."
                )
            )

        if intent.name == "utility.date":
            return SkillResult(
                True,
                now.strftime(
                    "Hôm nay là ngày %d tháng %m năm %Y."
                )
            )

        if intent.name == "utility.lunar_date":
            d, m, y, leap = solar_to_lunar(
                now.day,
                now.month,
                now.year,
                7
            )

            leap_text = " nhuận" if leap else ""

            return SkillResult(
                True,
                (
                    f"Hôm nay là ngày {d} "
                    f"tháng {m}{leap_text} "
                    f"năm {y} âm lịch."
                )
            )

        if intent.name == "volume.up":
            return SkillResult(
                True,
                "Đã tăng âm lượng.",
                {"command": "volume_up"}
            )

        if intent.name == "volume.down":
            return SkillResult(
                True,
                "Đã giảm âm lượng.",
                {"command": "volume_down"}
            )

        if intent.name == "audio.mute":
            return SkillResult(
                True,
                "Đã tắt micro.",
                {"command": "mute"}
            )

        if intent.name == "audio.unmute":
            return SkillResult(
                True,
                "Đã bật micro.",
                {"command": "unmute"}
            )

        return SkillResult(False)


MANIFEST = {
    "name": "utility",

    "intents": {
        "utility.time": {
            "phrases": [
                "mấy giờ rồi",
                "mấy giờ",
                "bây giờ là mấy giờ",
                "bây giờ mấy giờ",
                "giờ hiện tại",
            ]
        },

        "utility.date": {
            "phrases": [
                "hôm nay ngày mấy",
                "hôm nay là ngày mấy",
                "ngày dương hôm nay",
                "dương lịch hôm nay",
            ]
        },

        "utility.lunar_date": {
            "phrases": [
                "lịch âm hôm nay",
                "âm lịch hôm nay",
                "hôm nay âm lịch",
                "hôm nay là ngày mấy âm lịch",
                "âm lịch ngày mấy",
                "ngày âm hôm nay",
            ]
        },

        "volume.up": {
            "phrases": [
                "tăng âm lượng",
                "tăng tiếng",
                "to lên",
                "lớn tiếng hơn",
            ]
        },

        "volume.down": {
            "phrases": [
                "giảm âm lượng",
                "giảm tiếng",
                "nhỏ lại",
                "bé tiếng",
            ]
        },

        "audio.mute": {
            "phrases": [
                "tắt mic",
                "tắt micro",
                "mute mic",
                "im mic",
            ]
        },

        "audio.unmute": {
            "phrases": [
                "mở mic",
                "bật mic",
                "unmute mic",
            ]
        },
    },
}
