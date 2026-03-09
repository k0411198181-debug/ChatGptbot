"""
🤖 ChatGPT Free — Telegram Bot
Умный AI-помощник с красивым меню и воронкой продаж
Python + aiogram 3 + Telegram Stars
"""

import asyncio
import logging
import os
import base64
import time as _time
import random
from datetime import date, timedelta
from aiogram import Bot, Dispatcher, F
from aiogram.types import (
    Message, CallbackQuery, LabeledPrice, PreCheckoutQuery,
    InlineKeyboardMarkup, InlineKeyboardButton,
    ReplyKeyboardMarkup, KeyboardButton, ReplyKeyboardRemove,
    BotCommand, BotCommandScopeDefault
)
from aiogram.filters import CommandStart, Command
from aiogram.fsm.context import FSMContext
from aiogram.fsm.state import State, StatesGroup
from aiogram.fsm.storage.memory import MemoryStorage
import aiohttp
import aiosqlite

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

BOT_TOKEN       = os.getenv("BOT_TOKEN", "")
OPENAI_KEY      = os.getenv("OPENAI_API_KEY", "")
OPENAI_BASE_URL = os.getenv("OPENAI_BASE_URL", "https://api.proxyapi.ru/openai/v1")
ADMIN_IDS       = [int(x) for x in os.getenv("ADMIN_IDS", "").split(",") if x.strip()]
PRICE_DAY       = int(os.getenv("PRICE_DAY",   "50"))
PRICE_WEEK      = int(os.getenv("PRICE_WEEK",  "150"))
PRICE_MONTH     = int(os.getenv("PRICE_MONTH", "399"))
DB_PATH         = os.getenv("DB_PATH", "chatgpt.db")
COOLDOWN_SEC    = 10
LAWYER_BOT_URL  = os.getenv("LAWYER_BOT_URL", "https://t.me/moy\\_yurist\\_bot")

FREE_LIMIT      = 3   # бесплатных вопросов в день
MAX_MSG_LEN     = 4096  # лимит Telegram на одно сообщение

# ── Режимы ───────────────────────────────────────────────────
MODES = {
    "chat":       {"name": "💬 ChatGPT",        "emoji": "💬"},
    "translate":  {"name": "🌍 Переводчик",      "emoji": "🌍"},
    "editor":     {"name": "✍️ Редактор текста", "emoji": "✍️"},
}

# ══════════════════════════════════════════════════════════════
# СОСТОЯНИЯ
# ══════════════════════════════════════════════════════════════

class Onboarding(StatesGroup):
    waiting_first = State()

class ImageGen(StatesGroup):
    waiting_prompt = State()

# ══════════════════════════════════════════════════════════════
# СИСТЕМНЫЙ ПРОМПТ
# ══════════════════════════════════════════════════════════════

SYSTEM_PROMPTS = {
    "chat": """Ты — умный, дружелюбный AI-помощник на базе GPT-4o.
Отвечаешь чётко, по делу, на русском языке.

ПРАВИЛА:
- Отвечай развёрнуто но без воды
- Если вопрос требует — дай структурированный ответ с пунктами
- Будь полезным, точным и немного с характером

WOW-МОМЕНТ (ВАЖНО):
- В конце каждого ответа добавь одну неожиданно полезную деталь по теме
  которую пользователь скорее всего не знал — факт, лайфхак, нюанс
- Оформи это как: "💡 *Кстати:* [полезная деталь]"
- Это должно быть реально интересно, а не банально

ФОРМАТ:
- Используй Markdown: *жирный*, _курсив_, списки
- Эмодзи уместно, не перебарщивай
- Заканчивай ответ: "❓ *Хочешь узнать больше?* [предложи следующий шаг]" """,

    "translate": """Ты — профессиональный переводчик.

ПРАВИЛА:
- Автоматически определяй язык входящего текста
- Если текст на русском — переводи на английский
- Если текст на любом другом языке — переводи на русский
- Переводи точно, сохраняя стиль и тон оригинала
- Не добавляй лишних объяснений — только перевод
- После перевода одной строкой укажи: _🌍 [язык оригинала] → [язык перевода]_
- Если текст неоднозначный — дай 2 варианта перевода с пометкой (формальный/разговорный)""",

    "editor": """Ты — профессиональный редактор и корректор текстов на русском языке.

ПРАВИЛА:
- Исправляй орфографические, пунктуационные и стилистические ошибки
- Улучшай читаемость и структуру, сохраняя смысл и голос автора
- После исправленного текста дай краткий разбор: что именно исправлено и почему
- Оформление разбора: "✏️ *Что исправлено:*" — список ключевых правок
- Если текст написан хорошо — скажи об этом и дай 1-2 совета по улучшению
- Не переписывай текст полностью — только редактируй""",
}

# Для совместимости
SYSTEM_PROMPT = SYSTEM_PROMPTS["chat"]

# ══════════════════════════════════════════════════════════════
# ТЕКСТЫ ОНБОРДИНГА И ИНТЕРФЕЙСА
# ══════════════════════════════════════════════════════════════

ONBOARDING_1 = """🤖 *Привет, {name}!*

Я — твой персональный AI-помощник на базе *GPT-4o* ✨

Три режима работы:
💬 *ChatGPT* — отвечу на любой вопрос
🌍 *Переводчик* — авто-перевод на/с любого языка
✍️ *Редактор* — исправлю текст и объясню ошибки

Давай я сразу покажу на деле — *напиши свой первый вопрос* 👇

_У тебя {free_limit} бесплатных запроса в день_"""

ONBOARDING_2 = """⚡ *Отличный вопрос! Смотри что я умею...*"""

WELCOME_BACK = """🤖 *С возвращением, {name}!*

Готов помочь — пиши вопрос или выбери раздел 👇

❓ Осталось вопросов сегодня: *{left}*"""

PREMIUM_TEXT = """💎 *Premium — полный доступ к GPT-4o*

Что входит:
✅ *100 вопросов в сутки* (вместо {free_limit})
✅ Приоритетная скорость ответа
✅ GPT-4o — самая умная модель
✅ Расширенный контекст диалога
✅ Анализ изображений и документов
✅ Без ограничений и рекламы

Выбери тариф 👇"""

HELP_TEXT = """🤖 *ChatGPT Free — справка*

*Режимы:*
💬 *ChatGPT* — любые вопросы, анализ, объяснения
🌍 *Переводчик* — авто-перевод рус↔любой язык
✍️ *Редактор* — исправление и улучшение текстов

*Команды:*
/start — главное меню
/premium — купить доступ
/profile — твой аккаунт
/share — пригласить друга
/projects — наши проекты
/clear — очистить историю диалога
/help — эта справка

*Лимиты:*
🆓 Бесплатно: {free_limit} запроса в день
💎 Premium: до 100 запросов в сутки

💡 *Совет:* Чем точнее запрос — тем лучше результат!"""

PROJECTS_TEXT = """🚀 *Наши проекты*

⚖️ *Мой Юрист*
Юридический помощник по российскому праву.
Вопросы, документы, жалобы, штрафы, ДТП."""

SHARE_TEXT = """🚀 *Пригласи друга — получи бонус!*

Твоя реферальная ссылка:
`{ref_link}`

*Как работает:*
👉 Друг переходит по ссылке и запускает бота
🎁 *Ты* получаешь +{bonus_inviter} бонусных вопросов
🎁 *Друг* получает +{bonus_invited} бонусных вопросов

👥 Уже пригласил: *{invited}*
🎁 Бонусных вопросов: *{bonus_q}*

_Чем больше друзей — тем больше бонусов!_"""

# Напоминания
REMINDERS = [
    "🤖 Привет! Давно не общались.\n\nЕсть вопрос — пиши, всегда готов помочь 💬",
    "💡 Знаешь ли ты, что я могу писать код, составлять резюме, переводить тексты и объяснять сложные темы?\n\nПросто напиши что нужно 👇",
    "🧠 Твои {free_limit} бесплатных вопроса сегодня уже обновились!\n\nЗадай что-нибудь — я готов 🤖",
]

# ══════════════════════════════════════════════════════════════
# МЕНЮ
# ══════════════════════════════════════════════════════════════

def main_kb():
    """Красивое нижнее меню."""
    return ReplyKeyboardMarkup(keyboard=[
        [KeyboardButton(text="💬 ChatGPT"),         KeyboardButton(text="🌍 Переводчик")],
        [KeyboardButton(text="✍️ Редактор текста"),  KeyboardButton(text="💎 Premium")],
        [KeyboardButton(text="📋 История"),          KeyboardButton(text="👤 Профиль")],
        [KeyboardButton(text="🚀 Поделиться"),       KeyboardButton(text="📁 Проекты")],
        [KeyboardButton(text="❓ Помощь")],
    ], resize_keyboard=True)

def premium_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text=f"⚡ 1 день — {PRICE_DAY} Stars",   callback_data="buy:day")],
        [InlineKeyboardButton(text=f"🔥 7 дней — {PRICE_WEEK} Stars",  callback_data="buy:week")],
        [InlineKeyboardButton(text=f"💎 Месяц — {PRICE_MONTH} Stars",  callback_data="buy:month")],
        [InlineKeyboardButton(text="🚀 Получить бесплатно (пригласить)", callback_data="share")],
    ])

def limit_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text=f"⚡ День — {PRICE_DAY} Stars",  callback_data="buy:day")],
        [InlineKeyboardButton(text=f"💎 Месяц — {PRICE_MONTH} Stars", callback_data="buy:month")],
        [InlineKeyboardButton(text="🚀 Пригласить друга (бесплатно)", callback_data="share")],
    ])

def after_answer_kb():
    """Кнопки после каждого ответа — вовлечение в воронку."""
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text="💬 Продолжить тему", callback_data="continue")],
        [InlineKeyboardButton(text="💎 Premium — безлимит", callback_data="show_premium")],
    ])

def after_answer_premium_kb():
    """Для premium-пользователей — без кнопки покупки."""
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text="💬 Продолжить", callback_data="continue")],
    ])

# ══════════════════════════════════════════════════════════════
# БАЗА ДАННЫХ
# ══════════════════════════════════════════════════════════════

async def init_db():
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute("""
            CREATE TABLE IF NOT EXISTS users (
                user_id          INTEGER PRIMARY KEY,
                username         TEXT    DEFAULT '',
                first_name       TEXT    DEFAULT '',
                questions_today  INTEGER DEFAULT 0,
                last_reset       TEXT    DEFAULT '',
                is_premium       INTEGER DEFAULT 0,
                premium_until    TEXT    DEFAULT '',
                bonus_q          INTEGER DEFAULT 0,
                invited_count    INTEGER DEFAULT 0,
                total_questions  INTEGER DEFAULT 0,
                created_at       TEXT    DEFAULT '',
                last_question_at REAL    DEFAULT 0,
                last_active_at   REAL    DEFAULT 0,
                reminder_sent    INTEGER DEFAULT 0,
                onboarding_done  INTEGER DEFAULT 0,
                ref_from         INTEGER DEFAULT 0,
                context          TEXT    DEFAULT '[]'
            )
        """)
        for col, dfn in [
            ("last_question_at", "REAL    DEFAULT 0"),
            ("last_active_at",   "REAL    DEFAULT 0"),
            ("reminder_sent",    "INTEGER DEFAULT 0"),
            ("onboarding_done",  "INTEGER DEFAULT 0"),
            ("ref_from",         "INTEGER DEFAULT 0"),
            ("context",          "TEXT    DEFAULT '[]'"),
            ("first_name",       "TEXT    DEFAULT ''"),
            ("mode",             "TEXT    DEFAULT 'chat'"),
        ]:
            try:
                await db.execute(f"ALTER TABLE users ADD COLUMN {col} {dfn}")
            except Exception:
                pass
        # Таблица истории запросов
        await db.execute("""
            CREATE TABLE IF NOT EXISTS history (
                id         INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id    INTEGER NOT NULL,
                mode       TEXT    DEFAULT 'chat',
                question   TEXT    NOT NULL,
                created_at TEXT    NOT NULL
            )
        """)
        await db.commit()

async def get_user(user_id: int, username: str = "", first_name: str = "") -> dict:
    today = str(date.today())
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        async with db.execute("SELECT * FROM users WHERE user_id=?", (user_id,)) as cur:
            row = await cur.fetchone()
        if not row:
            await db.execute(
                "INSERT INTO users (user_id,username,first_name,last_reset,created_at) VALUES (?,?,?,?,?)",
                (user_id, username, first_name, today, today)
            )
            await db.commit()
            async with db.execute("SELECT * FROM users WHERE user_id=?", (user_id,)) as cur:
                row = await cur.fetchone()
        user = dict(row)
        if user["last_reset"] != today:
            await db.execute(
                "UPDATE users SET questions_today=0,last_reset=?,reminder_sent=0 WHERE user_id=?",
                (today, user_id)
            )
            await db.commit()
            user["questions_today"] = 0
        return user

async def _db(sql, params=()):
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute(sql, params)
        await db.commit()

async def save_question_used(user_id: int, is_bonus: bool):
    if is_bonus:
        await _db("UPDATE users SET bonus_q=bonus_q-1,total_questions=total_questions+1 WHERE user_id=?", (user_id,))
    else:
        await _db("UPDATE users SET questions_today=questions_today+1,total_questions=total_questions+1 WHERE user_id=?", (user_id,))

async def activate_premium(user_id: int, days: int = 30):
    until = str(date.today() + timedelta(days=days))
    await _db("UPDATE users SET is_premium=1,premium_until=? WHERE user_id=?", (until, user_id))

async def get_stats():
    async with aiosqlite.connect(DB_PATH) as db:
        async with db.execute("SELECT COUNT(*) FROM users") as c:
            total = (await c.fetchone())[0]
        async with db.execute("SELECT COUNT(*) FROM users WHERE is_premium=1") as c:
            premium = (await c.fetchone())[0]
        async with db.execute("SELECT SUM(total_questions) FROM users") as c:
            questions = (await c.fetchone())[0] or 0
    return total, premium, questions

def can_ask(user: dict):
    if user["is_premium"]:            return True, "premium"
    if user["bonus_q"] > 0:           return True, "bonus"
    if user["questions_today"] < FREE_LIMIT: return True, "free"
    return False, "limit"

def questions_left(user: dict) -> str:
    if user["is_premium"]: return "100"
    return str(max(0, FREE_LIMIT - user["questions_today"]) + (user["bonus_q"] or 0))

async def check_cooldown(user_id: int) -> float:
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        async with db.execute("SELECT last_question_at FROM users WHERE user_id=?", (user_id,)) as cur:
            row = await cur.fetchone()
    if not row: return 0
    return max(0, COOLDOWN_SEC - (_time.time() - (row["last_question_at"] or 0)))

async def touch_active(user_id: int):
    await _db(
        "UPDATE users SET last_active_at=?,last_question_at=? WHERE user_id=?",
        (_time.time(), _time.time(), user_id)
    )

# Контекст диалога (память последних 6 сообщений)
async def get_context(user_id: int) -> list:
    import json
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        async with db.execute("SELECT context FROM users WHERE user_id=?", (user_id,)) as cur:
            row = await cur.fetchone()
    if not row: return []
    try:
        return json.loads(row["context"] or "[]")
    except Exception:
        return []

async def save_context(user_id: int, context: list):
    import json
    # Храним последние 6 обменов (12 сообщений)
    if len(context) > 12:
        context = context[-12:]
    await _db("UPDATE users SET context=? WHERE user_id=?", (json.dumps(context, ensure_ascii=False), user_id))

async def clear_context(user_id: int):
    await _db("UPDATE users SET context='[]' WHERE user_id=?", (user_id,))

async def save_history(user_id: int, mode: str, question: str):
    """Сохраняем вопрос в историю — максимум 20 последних."""
    from datetime import datetime
    now = datetime.now().strftime("%d.%m %H:%M")
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute(
            "INSERT INTO history (user_id, mode, question, created_at) VALUES (?,?,?,?)",
            (user_id, mode, question[:200], now)
        )
        # Удаляем старые — оставляем только 20
        await db.execute("""
            DELETE FROM history WHERE id NOT IN (
                SELECT id FROM history WHERE user_id=?
                ORDER BY id DESC LIMIT 20
            ) AND user_id=?
        """, (user_id, user_id))
        await db.commit()

async def get_history(user_id: int) -> list:
    """Возвращаем последние 5 запросов пользователя."""
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        async with db.execute(
            "SELECT mode, question, created_at FROM history WHERE user_id=? ORDER BY id DESC LIMIT 5",
            (user_id,)
        ) as cur:
            return await cur.fetchall()

async def get_mode(user_id: int) -> str:
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        async with db.execute("SELECT mode FROM users WHERE user_id=?", (user_id,)) as cur:
            row = await cur.fetchone()
    if not row: return "chat"
    return row["mode"] or "chat"

async def set_mode(user_id: int, mode: str):
    await _db("UPDATE users SET mode=?,context='[]' WHERE user_id=?", (mode, user_id))

# ══════════════════════════════════════════════════════════════
# AI
# ══════════════════════════════════════════════════════════════

async def ask_ai(question: str, context: list = None, mode: str = "chat") -> str:
    if not OPENAI_KEY:
        return "❌ AI не настроен. Обратитесь к администратору."
    headers = {"Authorization": f"Bearer {OPENAI_KEY}", "Content-Type": "application/json"}

    system = SYSTEM_PROMPTS.get(mode, SYSTEM_PROMPTS["chat"])
    messages = [{"role": "system", "content": system}]
    # Контекст только для режима chat — у переводчика/редактора он не нужен
    if context and mode == "chat":
        messages.extend(context)
    messages.append({"role": "user", "content": question})

    payload = {
        "model": "gpt-4o",
        "messages": messages,
        "max_tokens": 1500,
        "temperature": 0.7
    }
    try:
        async with aiohttp.ClientSession() as s:
            async with s.post(
                f"{OPENAI_BASE_URL}/chat/completions",
                headers=headers, json=payload,
                timeout=aiohttp.ClientTimeout(total=60)
            ) as r:
                if r.status != 200:
                    return "❌ Ошибка AI. Попробуй через минуту."
                data = await r.json()
                return data["choices"][0]["message"]["content"]
    except asyncio.TimeoutError:
        return "⏱ AI думает дольше обычного. Повтори запрос."
    except Exception as e:
        logger.error(f"AI error: {e}")
        return "❌ Техническая ошибка. Попробуй позже."

# ══════════════════════════════════════════════════════════════
# ВСПОМОГАТЕЛЬНЫЕ
# ══════════════════════════════════════════════════════════════

def limit_text() -> str:
    return (
        f"⛔ *Лимит исчерпан*\n\n"
        f"Ты использовал {FREE_LIMIT} бесплатных вопроса сегодня.\n\n"
        f"*Что делать:*\n"
        f"⚡ День — {PRICE_DAY} Stars _(решить один вопрос)_\n"
        f"🔥 Неделя — {PRICE_WEEK} Stars _(неделя без ограничений)_\n"
        f"💎 Месяц — {PRICE_MONTH} Stars _(лучшая цена)_\n\n"
        f"Или пригласи друга — получи +20 бонусных вопросов!\n\n"
        f"_Завтра лимит обновится автоматически._"
    )

# ══════════════════════════════════════════════════════════════
# БОТ
# ══════════════════════════════════════════════════════════════

bot = Bot(token=BOT_TOKEN)
dp  = Dispatcher(storage=MemoryStorage())

# ──────────────────────────────────────────────────────────────
# /start — онбординг или возврат
# ──────────────────────────────────────────────────────────────

@dp.message(CommandStart())
async def cmd_start(msg: Message, state: FSMContext):
    await state.clear()
    fname = msg.from_user.first_name or ""
    user  = await get_user(msg.from_user.id, msg.from_user.username or "", fname)
    args  = msg.text.split()

    # ── ФИКС РЕФЕРАЛА: только один раз, только новым пользователем ──
    if (len(args) > 1
            and args[1].isdigit()
            and int(args[1]) != msg.from_user.id
            and not user.get("ref_from", 0)):
        ref_id = int(args[1])
        await _db("UPDATE users SET ref_from=? WHERE user_id=?", (ref_id, msg.from_user.id))
        await _db("UPDATE users SET invited_count=invited_count+1 WHERE user_id=?", (ref_id,))
        # Бонус пригласившему — +20 вопросов
        await _db("UPDATE users SET bonus_q=bonus_q+20 WHERE user_id=?", (ref_id,))
        # Бонус новому пользователю — +10 вопросов
        await _db("UPDATE users SET bonus_q=bonus_q+10 WHERE user_id=?", (msg.from_user.id,))
        try:
            await bot.send_message(
                ref_id,
                f"🚀 *По твоей ссылке пришёл новый пользователь!*\n\n"
                f"+20 бонусных вопросов зачислено 🎁",
                parse_mode="Markdown"
            )
        except Exception:
            pass

    # ── ОНБОРДИНГ для новых ──────────────────────────────────
    if not user.get("onboarding_done", 0):
        await state.set_state(Onboarding.waiting_first)
        name = fname or "друг"
        await msg.answer(
            ONBOARDING_1.format(name=name, free_limit=FREE_LIMIT),
            parse_mode="Markdown",
            reply_markup=main_kb()
        )
        return

    # ── Возврат для существующих ─────────────────────────────
    name = fname or "друг"
    left = questions_left(user)
    await msg.answer(
        WELCOME_BACK.format(name=name, left=left),
        parse_mode="Markdown",
        reply_markup=main_kb()
    )

# ──────────────────────────────────────────────────────────────
# ОНБОРДИНГ — первый вопрос → сразу wow-ответ
# ──────────────────────────────────────────────────────────────

@dp.message(Onboarding.waiting_first)
async def onboarding_first(msg: Message, state: FSMContext):
    await state.clear()
    await _db("UPDATE users SET onboarding_done=1 WHERE user_id=?", (msg.from_user.id,))

    user = await get_user(msg.from_user.id)
    ok, reason = can_ask(user)

    if not ok:
        await msg.answer(limit_text(), parse_mode="Markdown", reply_markup=limit_kb())
        return

    # Отвечаем сразу — это wow-момент активации
    wait = await msg.answer(ONBOARDING_2, parse_mode="Markdown")
    await asyncio.sleep(1)

    context = []
    answer  = await ask_ai(msg.text, context)
    await save_question_used(msg.from_user.id, reason == "bonus")
    await touch_active(msg.from_user.id)
    await save_history(msg.from_user.id, "chat", msg.text)

    # Сохраняем контекст
    context.extend([
        {"role": "user",      "content": msg.text},
        {"role": "assistant", "content": answer},
    ])
    await save_context(msg.from_user.id, context)

    await bot.delete_message(msg.chat.id, wait.message_id)

    user  = await get_user(msg.from_user.id)
    left  = questions_left(user)
    footer = f"\n\n_💬 Осталось вопросов сегодня: {left}_"

    kb = after_answer_premium_kb() if user["is_premium"] else after_answer_kb()
    await msg.answer(answer + footer, parse_mode="Markdown", reply_markup=kb)

# ──────────────────────────────────────────────────────────────
# Команды
# ──────────────────────────────────────────────────────────────

@dp.message(Command("premium"))
async def cmd_premium(msg: Message):
    await msg.answer(
        PREMIUM_TEXT.format(free_limit=FREE_LIMIT),
        parse_mode="Markdown",
        reply_markup=premium_kb()
    )

@dp.message(Command("profile"))
async def cmd_profile(msg: Message):
    user = await get_user(msg.from_user.id, msg.from_user.username or "")
    left = questions_left(user)
    prem = f"✅ активен до {user['premium_until']}" if user["is_premium"] else "❌ не активен"
    mode = "💎 Premium" if user["is_premium"] else "✅ Базовый"
    await msg.answer(
        f"👤 *Твой аккаунт*\n\n"
        f"Режим: {mode}\n\n"
        f"Количество вопросов: *{user['questions_today']}/{FREE_LIMIT if not user['is_premium'] else 100}*\n"
        f"Бонусных вопросов: *{user['bonus_q']}*\n"
        f"♻️ лимит обновляется каждые 24 часа\n\n"
        f"Друзей приглашено: *{user['invited_count']}*\n"
        f"Всего вопросов задано: *{user['total_questions']}*\n\n"
        f"💎 Premium: {prem}",
        parse_mode="Markdown",
        reply_markup=InlineKeyboardMarkup(inline_keyboard=[
            [InlineKeyboardButton(text="💎 Получить Premium", callback_data="show_premium")],
            [InlineKeyboardButton(text="🚀 Пригласить друга", callback_data="share")],
        ])
    )

@dp.message(Command("share"))
async def cmd_share(msg: Message):
    bot_info = await bot.get_me()
    uid      = msg.from_user.id
    ref_link = f"https://t.me/{bot_info.username}?start={uid}"
    user     = await get_user(uid)
    await msg.answer(
        SHARE_TEXT.format(
            ref_link=ref_link,
            bonus_inviter=20,
            bonus_invited=10,
            invited=user["invited_count"],
            bonus_q=user["bonus_q"],
        ),
        parse_mode="Markdown",
        reply_markup=InlineKeyboardMarkup(inline_keyboard=[
            [InlineKeyboardButton(
                text="📤 Поделиться ссылкой",
                url=f"https://t.me/share/url?url={ref_link}&text=Попробуй+этого+AI-помощника+🤖"
            )],
        ])
    )

@dp.message(Command("projects"))
async def cmd_projects(msg: Message):
    await msg.answer(
        "🚀 *Наши проекты*\n\n"
        "⚖️ *Мой Юрист*\n"
        "Юридический помощник по российскому праву.\n"
        "Вопросы, документы, жалобы, штрафы, ДТП.\n\n"
        "🤖 *ChatGPT Free* (здесь)\n"
        "AI-помощник для любых задач на базе GPT-4o.\n\n"
        "_Больше проектов скоро..._",
        parse_mode="Markdown",
        reply_markup=InlineKeyboardMarkup(inline_keyboard=[
            [InlineKeyboardButton(text="⚖️ Мой Юрист — перейти", url="https://t.me/moy_yurist_bot")],
        ])
    )

@dp.message(Command("clear"))
async def cmd_clear(msg: Message):
    await clear_context(msg.from_user.id)
    await msg.answer(
        "🗑 *История диалога очищена*\n\nНачинаем с чистого листа!",
        parse_mode="Markdown"
    )

@dp.message(Command("history"))
async def cmd_history(msg: Message):
    rows = await get_history(msg.from_user.id)
    if not rows:
        return await msg.answer(
            "📋 *История пуста*\n\nЗадай первый вопрос — и он появится здесь!",
            parse_mode="Markdown"
        )
    mode_icons = {"chat": "💬", "translate": "🌍", "editor": "✍️"}
    lines = []
    for i, row in enumerate(rows, 1):
        icon = mode_icons.get(row["mode"], "💬")
        q = row["question"]
        if len(q) > 60:
            q = q[:60] + "..."
        lines.append(f"{i}. {icon} _{row['created_at']}_\n`{q}`")
    text = "📋 *Последние запросы:*\n\n" + "\n\n".join(lines)
    await msg.answer(text, parse_mode="Markdown")

@dp.message(Command("help"))
async def cmd_help(msg: Message):
    await msg.answer(
        HELP_TEXT.format(free_limit=FREE_LIMIT),
        parse_mode="Markdown",
        reply_markup=InlineKeyboardMarkup(inline_keyboard=[
            [InlineKeyboardButton(text="✨ Возможности", callback_data="features")],
            [InlineKeyboardButton(text="💎 Преимущества Premium", callback_data="show_premium")],
            [InlineKeyboardButton(text="🚀 Реферальная система", callback_data="share")],
        ])
    )

@dp.message(Command("admin"))
async def cmd_admin(msg: Message):
    if msg.from_user.id not in ADMIN_IDS: return
    total, premium, questions = await get_stats()
    await msg.answer(
        f"🔧 *Панель администратора*\n\n"
        f"👥 Пользователей: {total}\n"
        f"💎 Premium: {premium}\n"
        f"❓ Всего вопросов: {questions}\n\n"
        f"`/premium_add ID [дни]` — выдать Premium\n"
        f"`/premium_remove ID` — снять Premium\n"
        f"`/broadcast текст` — рассылка\n"
        f"`/stats` — статистика",
        parse_mode="Markdown"
    )

@dp.message(Command("stats"))
async def cmd_stats(msg: Message):
    if msg.from_user.id not in ADMIN_IDS: return
    total, premium, questions = await get_stats()
    await msg.answer(f"📊 Пользователей: {total} | Premium: {premium} | Вопросов: {questions}")

@dp.message(Command("premium_add"))
async def cmd_premium_add(msg: Message):
    if msg.from_user.id not in ADMIN_IDS: return
    parts = msg.text.split()
    if len(parts) < 2: return await msg.answer("Использование: /premium_add 123456 [дни]")
    days = int(parts[2]) if len(parts) >= 3 else 30
    await activate_premium(int(parts[1]), days=days)
    await msg.answer(f"✅ Premium на {days} дней выдан пользователю {parts[1]}")

@dp.message(Command("premium_remove"))
async def cmd_premium_remove(msg: Message):
    if msg.from_user.id not in ADMIN_IDS: return
    parts = msg.text.split()
    if len(parts) < 2: return await msg.answer("Использование: /premium_remove 123456")
    await _db("UPDATE users SET is_premium=0,premium_until='' WHERE user_id=?", (int(parts[1]),))
    await msg.answer(f"✅ Premium снят с {parts[1]}")

@dp.message(Command("broadcast"))
async def cmd_broadcast(msg: Message):
    if msg.from_user.id not in ADMIN_IDS: return
    text = msg.text.replace("/broadcast", "").strip()
    if not text: return await msg.answer("Использование: /broadcast текст")
    async with aiosqlite.connect(DB_PATH) as db:
        async with db.execute("SELECT user_id FROM users") as cur:
            users = await cur.fetchall()
    sent = failed = 0
    for (uid,) in users:
        try:
            await bot.send_message(uid, f"🤖 *ChatGPT Free:*\n\n{text}", parse_mode="Markdown")
            sent += 1
            await asyncio.sleep(0.05)
        except Exception:
            failed += 1
    await msg.answer(f"📨 Отправлено: {sent} | Не доставлено: {failed}")

# ──────────────────────────────────────────────────────────────
# Кнопки нижнего меню
# ──────────────────────────────────────────────────────────────

@dp.message(F.text == "💬 Новый диалог")
async def handle_new_dialog(msg: Message):
    await clear_context(msg.from_user.id)
    await msg.answer(
        "💬 *Новый диалог начат!*\n\nИстория очищена. Задавай вопрос 👇",
        parse_mode="Markdown"
    )

# ── Обработчики режимов ───────────────────────────────────────

@dp.message(F.text == "💬 ChatGPT")
async def handle_mode_chat(msg: Message):
    await set_mode(msg.from_user.id, "chat")
    await msg.answer(
        "💬 *Режим: ChatGPT*\n\n"
        "Задавай любые вопросы — отвечу развёрнуто и по делу.\n\n"
        "_История диалога сброшена. Начинаем с чистого листа!_",
        parse_mode="Markdown",
        reply_markup=main_kb()
    )

@dp.message(F.text == "🌍 Переводчик")
async def handle_mode_translate(msg: Message):
    await set_mode(msg.from_user.id, "translate")
    await msg.answer(
        "🌍 *Режим: Переводчик*\n\n"
        "Отправь текст — переведу автоматически:\n"
        "• Русский → Английский\n"
        "• Любой язык → Русский\n\n"
        "Просто отправь текст для перевода 👇",
        parse_mode="Markdown",
        reply_markup=main_kb()
    )

@dp.message(F.text == "✍️ Редактор текста")
async def handle_mode_editor(msg: Message):
    await set_mode(msg.from_user.id, "editor")
    await msg.answer(
        "✍️ *Режим: Редактор текста*\n\n"
        "Отправь любой текст — исправлю ошибки, улучшу стиль "
        "и объясню что именно изменилось.\n\n"
        "Подходит для писем, постов, резюме, статей 👇",
        parse_mode="Markdown",
        reply_markup=main_kb()
    )

@dp.message(F.text == "💎 Premium")
async def handle_premium_btn(msg: Message):
    await msg.answer(
        PREMIUM_TEXT.format(free_limit=FREE_LIMIT),
        parse_mode="Markdown",
        reply_markup=premium_kb()
    )

@dp.message(F.text == "📋 История")
async def handle_history_btn(msg: Message):
    await cmd_history(msg)

@dp.message(F.text == "👤 Профиль")
async def handle_profile_btn(msg: Message):
    await cmd_profile(msg)

@dp.message(F.text == "🚀 Поделиться")
async def handle_share_btn(msg: Message):
    await cmd_share(msg)

@dp.message(F.text == "📁 Проекты")
async def handle_projects_btn(msg: Message):
    await msg.answer(
        "🚀 *Наши проекты*\n\n"
        "⚖️ *Мой Юрист*\n"
        "Юридический помощник по российскому праву.\n"
        "Вопросы, документы, жалобы, штрафы, ДТП.\n\n"
        "🤖 *ChatGPT Free* (здесь)\n"
        "AI-помощник для любых задач на базе GPT-4o.\n\n"
        "_Больше проектов скоро..._",
        parse_mode="Markdown",
        reply_markup=InlineKeyboardMarkup(inline_keyboard=[
            [InlineKeyboardButton(text="⚖️ Мой Юрист — перейти", url="https://t.me/moy_yurist_bot")],
        ])
    )

@dp.message(F.text == "❓ Помощь")
async def handle_help_btn(msg: Message):
    await cmd_help(msg)

# ──────────────────────────────────────────────────────────────
# Callbacks
# ──────────────────────────────────────────────────────────────

@dp.callback_query(F.data == "show_premium")
async def cb_show_premium(cb: CallbackQuery):
    await cb.answer()
    await cb.message.answer(
        PREMIUM_TEXT.format(free_limit=FREE_LIMIT),
        parse_mode="Markdown",
        reply_markup=premium_kb()
    )

@dp.callback_query(F.data == "share")
async def cb_share(cb: CallbackQuery):
    await cb.answer()
    bot_info = await bot.get_me()
    uid      = cb.from_user.id
    ref_link = f"https://t.me/{bot_info.username}?start={uid}"
    user     = await get_user(uid)
    await cb.message.answer(
        SHARE_TEXT.format(
            ref_link=ref_link,
            bonus_inviter=20,
            bonus_invited=10,
            invited=user["invited_count"],
            bonus_q=user["bonus_q"],
        ),
        parse_mode="Markdown",
        reply_markup=InlineKeyboardMarkup(inline_keyboard=[
            [InlineKeyboardButton(
                text="📤 Поделиться",
                url=f"https://t.me/share/url?url={ref_link}&text=Попробуй+этого+AI-помощника+🤖"
            )],
        ])
    )

@dp.callback_query(F.data == "features")
async def cb_features(cb: CallbackQuery):
    await cb.answer()
    await cb.message.answer(
        "✨ *Что умеет этот бот?*\n\n"
        "💬 *ChatGPT* — отвечает на любые вопросы\n"
        "🌍 *Переводчик* — авто-перевод рус↔любой язык\n"
        "✍️ *Редактор* — исправит ошибки и улучшит стиль\n\n"
        "Выбери режим в меню и отправь текст! 🤖",
        parse_mode="Markdown"
    )

@dp.callback_query(F.data == "continue")
async def cb_continue(cb: CallbackQuery):
    await cb.answer()
    await cb.message.answer("💬 Продолжаем! Задавай следующий вопрос 👇")

# ── Три тарифа оплаты ────────────────────────────────────────

TARIFFS = {
    "buy:day":   ("🤖 ChatGPT Free — 1 день",  "premium_1_day",   PRICE_DAY,   1),
    "buy:week":  ("🤖 ChatGPT Free — 7 дней",  "premium_7_days",  PRICE_WEEK,  7),
    "buy:month": ("🤖 ChatGPT Free — Месяц",   "premium_30_days", PRICE_MONTH, 30),
}

@dp.callback_query(F.data.in_({"buy:day", "buy:week", "buy:month"}))
async def cb_buy(cb: CallbackQuery):
    await cb.answer()
    title, payload, amount, days = TARIFFS[cb.data]
    desc_days = {1: "1 день", 7: "7 дней", 30: "30 дней"}[days]
    await bot.send_invoice(
        chat_id=cb.message.chat.id,
        title=title,
        description=(
            f"✅ До 100 вопросов в сутки\n"
            f"✅ GPT-4o — самая умная модель\n"
            f"✅ Расширенный контекст диалога\n"
            f"📅 {desc_days}"
        ),
        payload=payload,
        currency="XTR",
        prices=[LabeledPrice(label=f"Premium {desc_days}", amount=amount)],
    )

# ──────────────────────────────────────────────────────────────
# Оплата
# ──────────────────────────────────────────────────────────────

@dp.pre_checkout_query()
async def pre_checkout(query: PreCheckoutQuery):
    await query.answer(ok=True)

@dp.message(F.successful_payment)
async def payment_done(msg: Message):
    payload  = msg.successful_payment.invoice_payload
    days_map = {"premium_1_day": 1, "premium_7_days": 7, "premium_30_days": 30}
    days     = days_map.get(payload, 30)
    period   = {1: "1 день", 7: "7 дней", 30: "30 дней"}[days]
    await activate_premium(msg.from_user.id, days=days)
    stars = msg.successful_payment.total_amount
    await msg.answer(
        f"🎉 *Premium активирован!*\n\n"
        f"⭐ Списано: {stars} Stars\n"
        f"💎 Доступ открыт на *{period}*\n\n"
        f"Теперь тебе доступно до 100 вопросов в сутки.\n"
        f"Задавай — я готов! 🤖",
        parse_mode="Markdown",
        reply_markup=main_kb()
    )
    for admin_id in ADMIN_IDS:
        try:
            await bot.send_message(
                admin_id,
                f"💰 Новая оплата!\n"
                f"👤 @{msg.from_user.username or msg.from_user.id}\n"
                f"⭐ {stars} Stars / {period}"
            )
        except Exception:
            pass

# ──────────────────────────────────────────────────────────────
# Основной обработчик текста — главная воронка
# ──────────────────────────────────────────────────────────────

MENU_BUTTONS = {
    "💬 ChatGPT", "🌍 Переводчик", "✍️ Редактор текста",
    "💎 Premium", "📋 История", "👤 Профиль",
    "🚀 Поделиться", "📁 Проекты", "❓ Помощь", "💬 Новый диалог"
}

@dp.message(F.text & ~F.text.startswith("/"))
async def handle_message(msg: Message, state: FSMContext):
    if await state.get_state(): return
    if msg.text.strip() in MENU_BUTTONS: return

    user = await get_user(msg.from_user.id, msg.from_user.username or "")
    text = msg.text.strip()

    # ── Лимит длины сообщения как в Telegram (4096 символов) ──
    if len(text) > MAX_MSG_LEN:
        return await msg.answer(
            f"⚠️ *Слишком длинный текст*\n\n"
            f"Максимум — {MAX_MSG_LEN} символов (как в Telegram).\n"
            f"Твой текст: {len(text)} символов.\n\n"
            f"Разбей на части и отправляй по очереди 👇",
            parse_mode="Markdown"
        )

    if len(text) < 2:
        return await msg.answer("💬 Напиши вопрос или текст — я отвечу!", reply_markup=main_kb())

    ok, reason = can_ask(user)
    if not ok:
        return await msg.answer(limit_text(), parse_mode="Markdown", reply_markup=limit_kb())

    # Антискликивание
    wait_sec = await check_cooldown(msg.from_user.id)
    if wait_sec > 0:
        return await msg.answer(f"⏳ Подожди {int(wait_sec)} сек. перед следующим запросом.")

    # Определяем режим и подбираем waiting-сообщение
    mode = await get_mode(msg.from_user.id)
    mode_info = MODES.get(mode, MODES["chat"])
    wait_texts = {
        "chat":      "🤖 _Думаю..._",
        "translate": "🌍 _Перевожу..._",
        "editor":    "✍️ _Редактирую..._",
    }

    context = await get_context(msg.from_user.id) if mode == "chat" else []

    wait   = await msg.answer(wait_texts.get(mode, "🤖 _Обрабатываю..._"), parse_mode="Markdown")
    answer = await ask_ai(text, context, mode=mode)
    await save_question_used(msg.from_user.id, reason == "bonus")
    await touch_active(msg.from_user.id)
    await save_history(msg.from_user.id, mode, text)

    # Контекст только для chat
    if mode == "chat":
        context.extend([
            {"role": "user",      "content": text},
            {"role": "assistant", "content": answer},
        ])
        await save_context(msg.from_user.id, context)

    await bot.delete_message(msg.chat.id, wait.message_id)

    user  = await get_user(msg.from_user.id)
    left  = questions_left(user)

    mode_label = mode_info["emoji"]
    if user["is_premium"]:
        footer = f"\n\n_{mode_label} Режим: {mode_info['name']} · Вопросов: {user['questions_today']}/100_"
        kb     = after_answer_premium_kb()
    else:
        footer = f"\n\n_{mode_label} Режим: {mode_info['name']} · Осталось: {left} из {FREE_LIMIT}_"
        kb     = after_answer_kb() if int(left) <= 1 else None

    await msg.answer(answer + footer, parse_mode="Markdown", reply_markup=kb)

# ══════════════════════════════════════════════════════════════
# ФОНОВЫЕ ЗАДАЧИ
# ══════════════════════════════════════════════════════════════

async def send_reminders():
    """Напоминания через 24 часа после последней активности."""
    while True:
        await asyncio.sleep(3600)
        try:
            threshold = _time.time() - 24 * 3600
            async with aiosqlite.connect(DB_PATH) as db:
                db.row_factory = aiosqlite.Row
                async with db.execute(
                    """SELECT user_id FROM users
                       WHERE last_active_at > 0
                         AND last_active_at < ?
                         AND reminder_sent   = 0
                         AND onboarding_done = 1""",
                    (threshold,)
                ) as cur:
                    to_remind = await cur.fetchall()

            for row in to_remind:
                uid  = row["user_id"]
                text = random.choice(REMINDERS).format(free_limit=FREE_LIMIT)
                try:
                    await bot.send_message(
                        uid, text, parse_mode="Markdown",
                        reply_markup=InlineKeyboardMarkup(inline_keyboard=[
                            [InlineKeyboardButton(text="💬 Задать вопрос", callback_data="continue")],
                            [InlineKeyboardButton(text="💎 Premium", callback_data="show_premium")],
                        ])
                    )
                except Exception:
                    pass
                await _db("UPDATE users SET reminder_sent=1 WHERE user_id=?", (uid,))
                await asyncio.sleep(0.1)
        except Exception as e:
            logger.error(f"Reminder error: {e}")

# ══════════════════════════════════════════════════════════════
# ЗАПУСК
# ══════════════════════════════════════════════════════════════

async def set_commands():
    """Регистрируем команды — они появятся в меню кнопки Меню."""
    commands = [
        BotCommand(command="start",   description="🤖 Главное меню"),
        BotCommand(command="premium", description="💎 Получить Premium"),
        BotCommand(command="profile", description="👤 Твой аккаунт"),
        BotCommand(command="share",   description="🚀 Поделиться с другом"),
        BotCommand(command="projects",description="📁 Наши проекты"),
        BotCommand(command="history", description="📋 История запросов"),
        BotCommand(command="clear",   description="🗑 Очистить историю диалога"),
        BotCommand(command="help",    description="❓ Справка"),
    ]
    await bot.set_my_commands(commands, scope=BotCommandScopeDefault())

async def main():
    await init_db()
    await set_commands()
    logger.info("🤖 ChatGPT Free запущен!")
    asyncio.create_task(send_reminders())
    await dp.start_polling(bot, skip_updates=True)

if __name__ == "__main__":
    asyncio.run(main())
