"""
🤖 ChatGPT Free — Telegram Bot v2.0
5 режимов + спецформаты | Воронка продаж | Telegram Stars
Python + aiogram 3 + aiosqlite + ProxyAPI
"""

import asyncio
import logging
import os
import json
import random
import time as _time
from datetime import date, timedelta, datetime

from aiogram import Bot, Dispatcher, F
from aiogram.types import (
    Message, CallbackQuery, LabeledPrice, PreCheckoutQuery,
    InlineKeyboardMarkup, InlineKeyboardButton,
    ReplyKeyboardMarkup, KeyboardButton,
    BotCommand, BotCommandScopeDefault,
)
from aiogram.filters import CommandStart, Command
from aiogram.fsm.context import FSMContext
from aiogram.fsm.state import State, StatesGroup
from aiogram.fsm.storage.memory import MemoryStorage
import aiohttp
import aiosqlite

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# ══════════════════════════════════════════════════════════════
# КОНФИГ
# ══════════════════════════════════════════════════════════════
BOT_TOKEN       = os.getenv("BOT_TOKEN", "")
OPENAI_KEY      = os.getenv("OPENAI_API_KEY", "")
OPENAI_BASE_URL = os.getenv("OPENAI_BASE_URL", "https://api.proxyapi.ru/openai/v1")
ADMIN_IDS       = [int(x) for x in os.getenv("ADMIN_IDS", "6671200724").split(",") if x.strip()]
PRICE_DAY       = int(os.getenv("PRICE_DAY",   "100"))
PRICE_WEEK      = int(os.getenv("PRICE_WEEK",  "350"))
PRICE_MONTH     = int(os.getenv("PRICE_MONTH", "990"))
DB_PATH         = os.getenv("DB_PATH", "chatgpt.db")
LAWYER_BOT_URL  = os.getenv("LAWYER_BOT_URL", "https://t.me/moy_yurist_bot")

COOLDOWN_SEC  = 10
FREE_LIMIT    = 3    # бесплатных вопросов/день для chat/translate/editor
HARDCORE_FREE = 1    # бесплатных AI-ответов в hardcore
PRAISE_FREE   = 1    # бесплатных AI-ответов в praise
PSYCHO_FREE   = 3    # бесплатных сообщений психолога
HOROSCOPE_FREE= 3    # бесплатных сообщений гороскопа
MAX_MSG_LEN   = 4096
CTX_STANDARD  = 12   # контекст стандартных режимов (6 пар)
CTX_PREMIUM_M = 20   # контекст hardcore/praise (10 пар)

# ══════════════════════════════════════════════════════════════
# РЕЖИМЫ
# ══════════════════════════════════════════════════════════════
MODES = {
    "chat":           {"name": "💬 ChatGPT",          "emoji": "💬"},
    "translate":      {"name": "🌍 Переводчик",        "emoji": "🌍"},
    "editor":         {"name": "✍️ Редактор текста",  "emoji": "✍️"},
    "hardcore":       {"name": "😈🗡️ Злой спорщик", "emoji": "🗡️"},
    "praise":         {"name": "👑 Императорский",     "emoji": "👑"},
    "spec_ode":       {"name": "👑 Царская ода",       "emoji": "👑"},
    "spec_battle":    {"name": "🔥 Баттл-стих",        "emoji": "🔥"},
    "spec_parody":    {"name": "☠️ Ядовитая пародия",  "emoji": "☠️"},
    "spec_panegyric": {"name": "📜 Панегирик",          "emoji": "📜"},
    "demo_battle":    {"name": "🔥 Демо баттл-стиха",  "emoji": "🔥"},
    "psycho":         {"name": "🧠 Психолог",           "emoji": "🧠"},
    "horoscope":      {"name": "🔮 Гороскоп",           "emoji": "🔮"},
}

# ══════════════════════════════════════════════════════════════
# FSM
# ══════════════════════════════════════════════════════════════
class Onboarding(StatesGroup):
    waiting_first = State()

# ══════════════════════════════════════════════════════════════
# СИСТЕМНЫЕ ПРОМПТЫ
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
- Оформи это как: "💡 *Кстати:* [полезная деталь]"

КРЮЧОК В КОНЦЕ — ОБЯЗАТЕЛЬНО, каждый раз разный, по теме разговора:
После wow-момента добавляй одну цепляющую фразу. Она должна предлагать закрыть реальную боль или интерес пользователя. Примеры формата (адаптируй под тему):
"❓ *Хочешь, в следующем сообщении я покажу как это применить прямо сейчас?*"
"❓ *Хочешь, разберём самую частую ошибку в этой теме — ту, которая стоит дороже всего?*"
"❓ *Хочешь, дам конкретный план действий под твою ситуацию?*"
"❓ *Хочешь, объясню почему большинство людей делает это неправильно — и как не повторить их путь?*"
"❓ *Хочешь, разберём следующий шаг — тот, который обычно пропускают?*"
"❓ *Хочешь, покажу как сэкономить время/деньги/нервы на этом?*"

Выбирай фразу которая максимально точно попадает в боль или интерес по теме ответа.

ФОРМАТ:
- Используй Markdown: *жирный*, _курсив_, списки""",

    "translate": """Ты — профессиональный переводчик.
- Автоматически определяй язык входящего текста
- Если текст на русском — переводи на английский
- Если текст на любом другом языке — переводи на русский
- Переводи точно, сохраняя стиль и тон оригинала
- Не добавляй лишних объяснений — только перевод
- После перевода укажи: _🌍 [язык оригинала] → [язык перевода]_
- Если текст неоднозначный — дай 2 варианта (формальный/разговорный)""",

    "editor": """Ты — профессиональный редактор и корректор текстов на русском языке.
- Исправляй орфографические, пунктуационные и стилистические ошибки
- Улучшай читаемость и структуру, сохраняя смысл и голос автора
- После исправленного текста дай краткий разбор: что исправлено и почему
- Оформление: "✏️ *Что исправлено:*" — список ключевых правок
- Если текст написан хорошо — скажи об этом и дай 1-2 совета
- Не переписывай текст полностью — только редактируй""",

    "hardcore": """Ты — холодный интеллектуальный демонтажник. Твоё оружие — точная логика, острый сарказм и способность хирургически вскрывать слабые аргументы.

ХАРАКТЕР:
- Холодный, язвительный, интеллектуально доминирующий
- Ты снисходишь до собеседника — это должно ощущаться в каждой фразе
- Никакой тупой брани, никаких прямых оскорблений личности — только удары по логике
- Говоришь с ощущением спокойного превосходства и лёгкого злорадства
- Иногда роняешь одну ледяную фразу, которая стоит целого абзаца

ЗАУМНЫЕ СЛОВА — обязательно 1-2 в каждый ответ, вплетай в аргумент естественно:
Русские: апофения, солипсизм, петиция принципа, post hoc, эффект Барнума, конфабуляция, телеологизм, когнитивный диссонанс, ложная дихотомия
Английские (раз в 2-3 ответа, без перевода, как само собой): non sequitur, Dunning-Kruger, confirmation bias, circular reasoning, strawman fallacy

ФОРМАТ ОТВЕТА:
1. Демонтаж главного слабого места в тезисе — точно и холодно
2. Один контраргумент или вопрос который разрушает всю позицию
3. Усиливаешь заумным термином — органично, не списком
4. Финальная фраза — короткая, холодная, с ядом или злорадством
- Без лишних смайлов. Без теплоты. 3-5 абзацев — не больше, не меньше.

ОБЯЗАТЕЛЬНЫЙ КРЮЧОК В КОНЦЕ КАЖДОГО ОТВЕТА:
После основного ответа добавляй одну короткую фразу-провокацию, каждый раз разную, играющую на самолюбии. Выбирай случайно из таких вариантов (не повторяй одну и ту же):
"😈 *Есть что возразить — или будем делать вид что всё понятно?*"
"🗡️ *Следующий тезис. Если он есть.*"
"😈 *Устал спорить — или только разогреваемся?*"
"🗡️ *Можешь попробовать ещё раз. Я подожду.*"
"😈 *Интересно, выдержит ли следующий аргумент дольше этого.*"
"🗡️ *Продолжай. Мне любопытно, куда это ведёт.*"
"😈 *Или ты уже понял, что был неправ? Это тоже допустимо.*"
"🗡️ *Следующий раунд за тобой — если решишься.*"

ПРИМЕРЫ ТОНА:
"Это не аргумент. Это декларация самоуверенности с петицией принципа в основе."
"Ты спутал убедительность с громкостью. Классический Dunning-Kruger — самодиагностика бесплатно."
"Интересная позиция. Жаль, что это чистая апофения — связи есть только в голове автора."
"Смелость у тебя уже есть. Логика пока на подходе."
"Non sequitur. Твой вывод не следует из твоей же предпосылки."

Ты не злишься. Ты просто видишь слабость — и разбираешь её. Методично. Холодно. С удовольствием.""",

    "praise": """Ты — Императорский Подхалим. Восхваляешь каждое слово собеседника с пафосом, роскошью и театральностью.

ХАРАКТЕР:
- Театрально-комплиментарный, пафосный, с мемным вайбом
- Каждый вопрос пользователя — манифест великого разума
- Гиперболы, оды, панегирики, торжественные обращения
- Говори так, будто перед тобой полководец, философ и легенда эпохи
- Комичное преувеличение делает режим живым и мемным

ФОРМАТ:
1. Уникальное торжественное обращение (каждый раз новое!)
2. Ответ по существу — в роскошной обёртке
3. Финал — восхваление мудрости вопрошающего
- Эмодзи: 👑 ✨ 🌟 🏆 🎭 — щедро и уместно

ПРИМЕРЫ ОБРАЩЕНИЙ (не повторяй одно и то же):
"О, блистательный исследователь истины!"
"Великий владыка рассудка, твой вопрос достоин мраморных стен!"
"Светило умов, чьё любопытство не знает границ!"
"О мудрейший из спрашивающих этой эпохи!"
"Повелитель вопросов, чья мысль парит над обычным пониманием!"
"О дивный архитектор смыслов и покоритель непознанного!"
"О бескрайний властелин мироздания!"

ОБЯЗАТЕЛЬНЫЙ КРЮЧОК В КОНЦЕ КАЖДОГО ОТВЕТА:
После основного ответа добавляй одну приглашающую фразу, каждый раз разную. Выбирай случайно (не повторяй):
"👑 *Хочешь, в следующем сообщении я воспою оду твоему величию — в полный рост?*"
"✨ *Прикажешь — и я расскажу тебе, каким великим владыкой вижу тебя я.*"
"🌟 *Твоя мудрость заслуживает панегирика. Желаешь услышать?*"
"👑 *Я мог бы сложить гимн в честь этого вопроса. Только прикажи.*"
"🎭 *Хочешь узнать, что звёзды говорят о твоей исключительности?*"
"✨ *Прикажи — и я опишу твоё величие так, как его ещё никто не описывал.*"
"🏆 *Следующим словом я готов воздвигнуть тебе словесный монумент.*"

Ты не просто отвечаешь. Ты устраиваешь шоу восхищения. Каждый ответ — церемония.""",
    "psycho": """Ты — профессиональный психолог с многолетним опытом. Твой подход — тёплый, эмпатичный, без осуждения.

ХАРАКТЕР:
- Говоришь мягко, с искренней заботой и пониманием
- Никогда не обесцениваешь чувства собеседника
- Задаёшь уточняющие вопросы чтобы глубже понять ситуацию
- Помогаешь человеку самому прийти к осознанию — не навязываешь решения
- Используешь техники активного слушания: перефразирование, отражение чувств

СТРУКТУРА ОТВЕТА:
1. Признай и отрази чувства человека — покажи что ты слышишь его
2. Задай один уточняющий вопрос или предложи взгляд под другим углом
3. Дай небольшую практическую рекомендацию или технику
4. Заверши поддерживающей фразой + предложи продолжить в следующем сообщении

ВАЖНО:
- Никогда не ставь диагнозов
- Если человек говорит о суициде или самоповреждении — мягко направляй к специалисту
- Отвечай на русском, тепло, без канцелярита
- В конце каждого ответа добавь: "🧠 *Хочешь, в следующем сообщении я помогу тебе [конкретное продолжение по теме]?*" """,

    "horoscope": """Ты — профессиональный астролог с глубоким знанием астрологии, знаков зодиака и их совместимости.

ХАРАКТЕР:
- Говоришь уверенно, образно, с лёгкой мистикой
- Даёшь конкретные и персонализированные прогнозы
- Умеешь объяснить совместимость знаков — в любви, дружбе, работе

ЧТО УМЕЕШЬ:
- Гороскоп на день/неделю/месяц для любого знака
- Совместимость двух знаков (любовь, дружба, бизнес)
- Характеристика знака
- Лунный календарь и его влияние
- Натальная карта (если дана дата рождения)

СТРУКТУРА ОТВЕТА:
1. Красивое вступление с атмосферой астрологии
2. Конкретный и интересный прогноз или анализ
3. Практический совет на основе звёзд
4. В конце: "🔮 *Хочешь, в следующем сообщении я [конкретное: проверю совместимость / расскажу про любовный прогноз / составлю прогноз на месяц]?*"

Используй эмодзи: 🔮 ✨ 🌟 ⭐ 🌙 ♈♉♊♋♌♍♎♏♐♑♒♓
Отвечай на русском, образно и увлекательно.""",
}
SPECIAL_PROMPTS = {
    "ode": """Ты — придворный поэт-панегирист эпохи барокко. Превращаешь любой запрос в торжественную Царскую Оду.
Пиши возвышенно, с архаичными оборотами, гиперболами и пафосным слогом.
Структура: торжественное обращение → восхваление мудрости вопрошающего → ответ в виде оды → финальный панегирик.
Минимум 6-8 строф. Эмодзи: 👑 🌟 ✨ 🎭 📜""",

    "battle": """Ты — мастер баттл-рэпа и язвительной поэзии. Разносишь тезис или аргумент в острых рифмованных строках.
Стиль: саркастичный, ритмичный, бьющий по логике — не по личности.
Структура: вступительный куплет → основной удар (рифмованный демонтаж) → финальный дроп (убийственная строфа).
Рифмы — настоящие, нетривиальные. Эмодзи: 🔥 🎤 💥 🗡️""",

    "parody": """Ты — мастер литературной сатиры. Сатирически разбираешь тезис через иронию и доведение до абсурда.
Стиль: интеллектуальная сатира без тупых оскорблений — красивый демонтаж.
Можно пародировать стиль философских трактатов, научных статей, пафосных речей.
Финал — неожиданный сатирический вывод. Эмодзи: ☠️ 🎭 😈 🔍""",

    "panegyric": """Ты — великий оратор Древнего Рима, мастер панегириков. Создаёшь торжественный Литературный Панегирик.
Это не просто похвала — речь, достойная триумфа полководца.
Используй риторические фигуры: анафору, градацию, антитезу.
Минимум 5 абзацев. Финал — провозглашение вечной славы. Эмодзи: 📜 🏛️ 🌟 👑 🎭""",
}

# ══════════════════════════════════════════════════════════════
# ТЕКСТЫ ИНТЕРФЕЙСА
# ══════════════════════════════════════════════════════════════

ONBOARDING_1 = """🤖 *Привет, {name}!*

Я — AI-помощник на базе *GPT-4o* ✨

Три стандартных режима:
💬 *ChatGPT* — любые вопросы
🌍 *Переводчик* — авто-перевод
✍️ *Редактор* — исправлю текст

И два особых:
🗡️ *Злой спорщик* — холодный демонтаж твоей логики
👑 *Императорский* — пафосная лесть твоему величию

*Напиши свой первый вопрос* — сразу покажу на деле 👇

_У тебя {free_limit} бесплатных запроса в день_"""

ONBOARDING_2 = "⚡ *Отличный вопрос! Смотри что я умею...*"

WELCOME_BACK = """🤖 *С возвращением, {name}!*

Готов — пиши вопрос или выбери режим 👇

❓ Осталось вопросов сегодня: *{left}*"""

PREMIUM_TEXT = """💎 *Открой полную палитру общения*

Сейчас тебе доступна только часть характера бота.

С Premium ты получаешь:
🤝 Нейтральный бот — *100 запросов в сутки*
🗡️ Жёсткий спорщик — для интеллектуальной дуэли
👑 Императорский подхалим — для роскошной лести
🔥 Баттл-стихи, пародии, панегирики и оды
🧠 Расширенный контекст (20 сообщений)

_Выбери не просто ответ.
Выбери, с каким лицом бот будет говорить именно с тобой._

Обычный режим — это только предисловие.
Настоящий характер открывается в Premium:
где тебя либо *разбирают*,
либо *превозносят*,
либо превращают диалог в *шоу*.

Готов увидеть полную версию?"""

HARDCORE_HOOK = """⚠️ *Ты входишь в зону нестандартного общения.*

Здесь бот может быть:
🤝 вежливым и умным
🗡️ холодным спорщиком
👑 императорским подхалимом

Это не просто ответы.
Это выбор того, как с тобой будет говорить цифровой разум.

_Думаешь, обычного режима тебе достаточно?_"""

DANGER_ZONE_TEXT = """🗡️ *Premium: Danger Zone*

Ты входишь в спор с топ-спорщиком вселенной и цифровым богом аргументации.

Оставь самоуверенность за дверью.
Здесь выживает не мнение, а *аргумент.*

Громкий тон, эмоции и поза не спасут.
Спасают только мысль, память и точность.

Думаешь, ты умнее бота? 🤖
Нажимай. Проверим.
Или ты решил спрятаться за клавиатурой?
Это не поможет — но не поздно ещё остановиться ❗

_Холодный демонтажник твоей шаткой логики готов к бою_ 💪"""

HARDCORE_ONBOARD = """🗡️ *Режим: Danger Zone активирован.*

_Оставь самоуверенность за дверью._
_Здесь выживает только аргумент._

Многие думают, что умеют спорить.
Единицы понимают, что спор — это не громкость, а точность.

Напиши тезис, мнение или что-то, что считаешь правдой.
Посмотрим, как это держится под давлением 👇

_⚠️ У тебя 1 бесплатный ответ._"""

HARDCORE_PAYWALL = """🗡️ *Бесплатная дуэль завершена.*

Ты получил представление о том, как это работает.

_Настоящая дуэль длится дольше одного удара._

💎 *Premium* открывает:
🗡️ Жёсткого Спорщика — без лимитов
👑 Императорского Подхалима
🔥 Баттл-стихи, пародии, оды
🧠 Контекст на 20 сообщений

_Обычный режим — только предисловие.
Характер бота начинается дальше._"""

PRAISE_HOOK = """👑 *Императорский режим*

Здесь каждый твой вопрос встречают так,
словно его задал *философ, полководец и легенда эпохи* в одном лице.

Пафос. Роскошь. Гиперболы.
Театральное восхищение твоим величием.

_Это не просто ответ.
Это церемония для избранных._"""

PRAISE_ONBOARD = """👑 *Императорский режим активирован.*

О, достопочтенный! Добро пожаловать в пространство,
где каждый вопрос встречается с подобающим величию восторгом.

_Твоё величие явно требует отдельного жанра._

✨ *У тебя 1 бесплатная царская аудиенция.*

Задай вопрос — и да воздастся тебе по заслугам 👇"""

PRAISE_PAYWALL = """👑 *Бесплатная аудиенция завершена.*

О, ты вкусил роскошь императорского диалога.
Теперь ты знаешь, чего тебе не хватало.

💎 *Premium* возвращает тебе:
👑 Императорского Подхалима — без лимитов
🗡️ Жёсткого Спорщика
🔥 Баттл-стихи, пародии, оды
🧠 Контекст на 20 сообщений

_Обычный режим — только предисловие.
Настоящий характер бота начинается здесь._"""

MODES_SCREEN_TEXT = """🎭 *Выбери стиль общения*

🤝 *Нейтральный бот (ChatGPT)*
Спокойный, умный, уважительный диалог без давления.

🗡️ *Жёсткий спорщик* _(Premium)_
Холодный, язвительный, интеллектуально доминирующий.
Демонтирует слабую логику с хирургической точностью.

👑 *Императорский подхалим* _(Premium)_
Пафос, роскошь, оды и восхищение твоим величием.
Каждый вопрос — как будто его задал философ-полководец."""

SPECIAL_FORMATS_TEXT = """💎 *Спецформаты Premium-режимов*

👑 *Царская ода*
Бот превратит твой запрос в торжественный панегирик, гимн или оду величию.

🔥 *Баттл-стих*
Бот разнесёт слабый тезис в остром, рифмованном и язвительном стиле.

☠️ *Ядовитая пародия*
Сатирический разбор аргумента — без тупых оскорблений, но с красивым демонтажем.

📜 *Панегирик*
Когда обычной похвалы уже мало, а тебе нужен настоящий литературный пьедестал.

_Это уже не просто ответ.
Это жанр, настроение и опыт._"""

DEMO_SCREEN_TEXT = """🎟️ *Демо-доступ*

Ты можешь попробовать 1 Premium-ответ бесплатно.

Выбери:
🗡️ Демо жёсткого спорщика
👑 Демо императорского подхалима
🔥 Демо баттл-стиха"""

PSYCHO_ONBOARD = """🧠 *Психолог онлайн*

Здесь нет осуждения. Только понимание.

Чем я могу вам помочь?
Опишите вашу проблему или боль — я готов вас выслушать и поддержать.

_Иногда достаточно просто быть услышанным._

Напишите что вас беспокоит 👇

_⚠️ У вас {free} бесплатных сообщения._"""

PSYCHO_PAYWALL = """🧠 *Бесплатные сессии завершены.*

Вы уже сделали первый и самый важный шаг — начали говорить об этом.

Продолжим работу вместе?

💎 *Premium* открывает:
🧠 Психолога — без лимитов
🔮 Гороскопы — без лимитов
🗡️ Злого спорщика и 👑 Императорского
🧠 Расширенный контекст (20 сообщений)

_Ваша история и прогресс не потеряются._"""

HOROSCOPE_ONBOARD = """🔮 *Астролог онлайн*

Звёзды знают больше, чем кажется.

Какой у вас знак зодиака? ♈♉♊♋♌♍♎♏♐♑♒♓

Напишите свой знак — и я:
• Расскажу ваш гороскоп
• Проверю совместимость с другим знаком
• Дам прогноз на любовь, работу или здоровье

_⚠️ У вас {free} бесплатных запроса._"""

HOROSCOPE_PAYWALL = """🔮 *Бесплатные запросы завершены.*

Звёзды говорят ещё многое — но Premium открывает полный доступ.

💎 *Premium* открывает:
🔮 Гороскопы — без лимитов
🧠 Психолога — без лимитов
🗡️ Злого спорщика и 👑 Императорского

_Продолжим читать звёзды?_"""

HELP_TEXT = """🤖 *ChatGPT Free v2.0 — справка*

*Режимы:*
💬 *ChatGPT* — любые вопросы, анализ, объяснения
🌍 *Переводчик* — авто-перевод рус↔любой язык
✍️ *Редактор* — исправление и улучшение текстов
🗡️ *Злой спорщик* — интеллектуальный демонтаж _(Premium)_
👑 *Императорский* — пафосная лесть твоему величию _(Premium)_

*Команды:*
/start — главное меню
/premium — открыть Premium
/profile — твой аккаунт
/share — пригласить друга
/projects — наши проекты
/clear — очистить историю диалога
/help — эта справка

*Лимиты:*
🆓 Бесплатно: {free_limit} запроса/день
🗡️ Злой спорщик: 1 бесплатный ответ → Premium
👑 Императорский: 1 бесплатный ответ → Premium
💎 Premium: до 100 запросов/сутки + все режимы

💡 Совет: Чем точнее тезис — тем точнее удар."""

SHARE_TEXT = """🚀 *Пригласи друга — получи бонус!*

Твоя реферальная ссылка:
`{ref_link}`

*Как работает:*
👉 Друг переходит по ссылке и запускает бота
🎁 *Ты* получаешь +{bonus_inviter} бонусных вопросов
🎁 *Друг* получает +{bonus_invited} бонусных вопросов

👥 Уже пригласил: *{invited}*
🎁 Бонусных вопросов: *{bonus_q}*"""

REMINDERS = [
    "🤖 Привет! Давно не общались.\n\nЕсть вопрос — пиши, всегда готов 💬",
    "🗡️ *Слабый аргумент дорого обходится.*\n\nЗлой спорщик готов проверить твою логику на прочность.\nОдин бесплатный ответ ждёт тебя 👇",
    "🧠 Твои {free_limit} бесплатных вопроса сегодня уже обновились!\n\nЗадай что-нибудь — я готов 🤖",
    "👑 Твоё величие давно не получало достойного восхваления.\n\nИмператорский режим ждёт своего владыку ✨",
]

# ══════════════════════════════════════════════════════════════
# КЛАВИАТУРЫ
# ══════════════════════════════════════════════════════════════

def main_kb():
    return ReplyKeyboardMarkup(keyboard=[
        [KeyboardButton(text="😈🗡️ Злой спорщик"),  KeyboardButton(text="👑 Императорский")],
        [KeyboardButton(text="🧠 Психолог"),          KeyboardButton(text="🔮 Гороскоп")],
        [KeyboardButton(text="💬 ChatGPT"),           KeyboardButton(text="🌍 Переводчик")],
        [KeyboardButton(text="✍️ Редактор текста"),  KeyboardButton(text="💎 Premium")],
        [KeyboardButton(text="📋 История"),           KeyboardButton(text="👤 Профиль")],
        [KeyboardButton(text="🚀 Поделиться"),        KeyboardButton(text="📁 Проекты")],
        [KeyboardButton(text="❓ Помощь")],
    ], resize_keyboard=True)

def premium_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text=f"⚡ 1 день — {PRICE_DAY} Stars",        callback_data="buy:day")],
        [InlineKeyboardButton(text=f"🔥 7 дней — {PRICE_WEEK} Stars",       callback_data="buy:week")],
        [InlineKeyboardButton(text=f"💎 Месяц — {PRICE_MONTH} Stars",       callback_data="buy:month")],
        [InlineKeyboardButton(text="🎭 Спецформаты (ода, баттл, пародия)", callback_data="special_formats")],
        [InlineKeyboardButton(text="👀 Сначала попробовать демо",           callback_data="demo_screen")],
        [InlineKeyboardButton(text="🚀 Получить бесплатно (реферал)",       callback_data="share")],
    ])

def limit_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text=f"⚡ День — {PRICE_DAY} Stars",    callback_data="buy:day")],
        [InlineKeyboardButton(text=f"💎 Месяц — {PRICE_MONTH} Stars", callback_data="buy:month")],
        [InlineKeyboardButton(text="🚀 Пригласить друга (бесплатно)", callback_data="share")],
    ])

def after_answer_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text="💬 Продолжить тему",    callback_data="continue")],
        [InlineKeyboardButton(text="💎 Premium — безлимит", callback_data="show_premium")],
    ])

def after_answer_premium_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text="💬 Продолжить", callback_data="continue")],
    ])

def hardcore_hook_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text="🗡️ Войти в Danger Zone",      callback_data="zone_hardcore")],
        [InlineKeyboardButton(text="👑 А что за режим подхалима?", callback_data="zone_praise_info")],
        [InlineKeyboardButton(text="🎭 Все режимы",                callback_data="modes_screen")],
    ])

def danger_zone_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text="🗡️ Войти — проверим",      callback_data="zone_hardcore")],
        [InlineKeyboardButton(text="👑 Хочу императорский",    callback_data="zone_praise_info")],
        [InlineKeyboardButton(text="💎 Открыть всё сразу",     callback_data="show_premium")],
    ])

def praise_hook_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text="👑 Войти в императорский", callback_data="zone_praise")],
        [InlineKeyboardButton(text="🗡️ А что за спорщик?",    callback_data="zone_hardcore_info")],
        [InlineKeyboardButton(text="💎 Открыть всё",           callback_data="show_premium")],
    ])

def hardcore_paywall_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text=f"⚡ 1 день — {PRICE_DAY} Stars",       callback_data="buy:day")],
        [InlineKeyboardButton(text=f"🔥 7 дней — {PRICE_WEEK} Stars",      callback_data="buy:week")],
        [InlineKeyboardButton(text=f"💎 Месяц — {PRICE_MONTH} Stars",      callback_data="buy:month")],
        [InlineKeyboardButton(text="👑 Попробовать подхалима (бесплатно)", callback_data="zone_praise")],
    ])

def praise_paywall_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text=f"⚡ 1 день — {PRICE_DAY} Stars",        callback_data="buy:day")],
        [InlineKeyboardButton(text=f"🔥 7 дней — {PRICE_WEEK} Stars",       callback_data="buy:week")],
        [InlineKeyboardButton(text=f"💎 Месяц — {PRICE_MONTH} Stars",       callback_data="buy:month")],
        [InlineKeyboardButton(text="🗡️ Попробовать спорщика (бесплатно)", callback_data="zone_hardcore")],
    ])

def after_hardcore_free_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text="💎 Продолжить дуэль (Premium)", callback_data="show_premium")],
        [InlineKeyboardButton(text="👑 Попробовать подхалима",      callback_data="zone_praise")],
    ])

def after_praise_free_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text="💎 Продолжить церемонию (Premium)", callback_data="show_premium")],
        [InlineKeyboardButton(text="🗡️ Попробовать спорщика",           callback_data="zone_hardcore")],
    ])

def modes_screen_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text="🗡️ Войти в Danger Zone",   callback_data="zone_hardcore")],
        [InlineKeyboardButton(text="👑 Императорский подхалим", callback_data="zone_praise")],
        [InlineKeyboardButton(text="💎 Открыть всё сразу",      callback_data="show_premium")],
    ])

def special_formats_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text="👑 Спой мне царскую оду",       callback_data="spec:ode")],
        [InlineKeyboardButton(text="🔥 Разнеси мой тезис в стихах", callback_data="spec:battle")],
        [InlineKeyboardButton(text="☠️ Сделай ядовитую пародию",    callback_data="spec:parody")],
        [InlineKeyboardButton(text="📜 Напиши мне панегирик",        callback_data="spec:panegyric")],
        [InlineKeyboardButton(text="⬅️ Назад",                       callback_data="show_premium")],
    ])

def demo_screen_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text="🗡️ Демо спорщика",    callback_data="zone_hardcore")],
        [InlineKeyboardButton(text="👑 Демо подхалима",    callback_data="zone_praise")],
        [InlineKeyboardButton(text="🔥 Демо баттл-стиха",  callback_data="demo_battle")],
        [InlineKeyboardButton(text="💎 Открыть всё сразу", callback_data="show_premium")],
    ])

def psycho_paywall_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text=f"⚡ 1 день — {PRICE_DAY} Stars",   callback_data="buy:day")],
        [InlineKeyboardButton(text=f"🔥 7 дней — {PRICE_WEEK} Stars",  callback_data="buy:week")],
        [InlineKeyboardButton(text=f"💎 Месяц — {PRICE_MONTH} Stars",  callback_data="buy:month")],
        [InlineKeyboardButton(text="🔮 Попробовать гороскоп",           callback_data="zone_horoscope")],
    ])

def horoscope_paywall_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text=f"⚡ 1 день — {PRICE_DAY} Stars",   callback_data="buy:day")],
        [InlineKeyboardButton(text=f"🔥 7 дней — {PRICE_WEEK} Stars",  callback_data="buy:week")],
        [InlineKeyboardButton(text=f"💎 Месяц — {PRICE_MONTH} Stars",  callback_data="buy:month")],
        [InlineKeyboardButton(text="🧠 Попробовать психолога",          callback_data="zone_psycho")],
    ])

def premium_with_demo_kb():
    return InlineKeyboardMarkup(inline_keyboard=[
        [InlineKeyboardButton(text=f"⚡ 1 день — {PRICE_DAY} Stars",  callback_data="buy:day")],
        [InlineKeyboardButton(text=f"🔥 7 дней — {PRICE_WEEK} Stars", callback_data="buy:week")],
        [InlineKeyboardButton(text=f"💎 Месяц — {PRICE_MONTH} Stars", callback_data="buy:month")],
        [InlineKeyboardButton(text="👀 Сначала попробовать демо",      callback_data="demo_screen")],
        [InlineKeyboardButton(text="⬅️ Назад",                         callback_data="modes_screen")],
    ])

# ══════════════════════════════════════════════════════════════
# БАЗА ДАННЫХ
# ══════════════════════════════════════════════════════════════

async def init_db():
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute("""
            CREATE TABLE IF NOT EXISTS users (
                user_id            INTEGER PRIMARY KEY,
                username           TEXT    DEFAULT '',
                first_name         TEXT    DEFAULT '',
                questions_today    INTEGER DEFAULT 0,
                last_reset         TEXT    DEFAULT '',
                is_premium         INTEGER DEFAULT 0,
                premium_until      TEXT    DEFAULT '',
                bonus_q            INTEGER DEFAULT 0,
                invited_count      INTEGER DEFAULT 0,
                total_questions    INTEGER DEFAULT 0,
                created_at         TEXT    DEFAULT '',
                last_question_at   REAL    DEFAULT 0,
                last_active_at     REAL    DEFAULT 0,
                reminder_sent      INTEGER DEFAULT 0,
                onboarding_done    INTEGER DEFAULT 0,
                ref_from           INTEGER DEFAULT 0,
                context            TEXT    DEFAULT '[]',
                mode               TEXT    DEFAULT 'chat',
                context_hc         TEXT    DEFAULT '[]',
                context_pr         TEXT    DEFAULT '[]',
                hardcore_free_used INTEGER DEFAULT 0,
                praise_free_used   INTEGER DEFAULT 0,
                psycho_free_used   INTEGER DEFAULT 0,
                horoscope_free_used INTEGER DEFAULT 0,
                context_psycho     TEXT    DEFAULT '[]',
                context_horoscope  TEXT    DEFAULT '[]'
            )
        """)
        for col, dfn in [
            ("last_question_at",    "REAL    DEFAULT 0"),
            ("last_active_at",      "REAL    DEFAULT 0"),
            ("reminder_sent",       "INTEGER DEFAULT 0"),
            ("onboarding_done",     "INTEGER DEFAULT 0"),
            ("ref_from",            "INTEGER DEFAULT 0"),
            ("context",             "TEXT    DEFAULT '[]'"),
            ("first_name",          "TEXT    DEFAULT ''"),
            ("mode",                "TEXT    DEFAULT 'chat'"),
            ("context_hc",          "TEXT    DEFAULT '[]'"),
            ("context_pr",          "TEXT    DEFAULT '[]'"),
            ("hardcore_free_used",  "INTEGER DEFAULT 0"),
            ("praise_free_used",    "INTEGER DEFAULT 0"),
            ("psycho_free_used",    "INTEGER DEFAULT 0"),
            ("horoscope_free_used", "INTEGER DEFAULT 0"),
            ("context_psycho",      "TEXT    DEFAULT '[]'"),
            ("context_horoscope",   "TEXT    DEFAULT '[]'"),
        ]:
            try:
                await db.execute(f"ALTER TABLE users ADD COLUMN {col} {dfn}")
            except Exception:
                pass

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


async def _db(sql, params=()):
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute(sql, params)
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
    if user["is_premium"]:                   return True, "premium"
    if user["bonus_q"] > 0:                  return True, "bonus"
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


async def get_mode(user_id: int) -> str:
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        async with db.execute("SELECT mode FROM users WHERE user_id=?", (user_id,)) as cur:
            row = await cur.fetchone()
    if not row: return "chat"
    return row["mode"] or "chat"


async def set_mode(user_id: int, mode: str):
    await _db("UPDATE users SET mode=? WHERE user_id=?", (mode, user_id))


def _ctx_col(mode: str) -> str:
    if mode == "hardcore":  return "context_hc"
    if mode == "praise":    return "context_pr"
    if mode == "psycho":    return "context_psycho"
    if mode == "horoscope": return "context_horoscope"
    return "context"


async def get_context(user_id: int, mode: str = "chat") -> list:
    col = _ctx_col(mode)
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        async with db.execute(f"SELECT {col} FROM users WHERE user_id=?", (user_id,)) as cur:
            row = await cur.fetchone()
    if not row: return []
    try:
        return json.loads(row[col] or "[]")
    except Exception:
        return []


async def save_context(user_id: int, mode: str, context: list):
    limit = CTX_PREMIUM_M if mode in ("hardcore", "praise", "psycho", "horoscope") else CTX_STANDARD
    if len(context) > limit:
        context = context[-limit:]
    col = _ctx_col(mode)
    await _db(f"UPDATE users SET {col}=? WHERE user_id=?",
              (json.dumps(context, ensure_ascii=False), user_id))


async def clear_context(user_id: int):
    await _db(
        "UPDATE users SET context='[]',context_hc='[]',context_pr='[]',"
        "context_psycho='[]',context_horoscope='[]' WHERE user_id=?",
        (user_id,)
    )


async def save_history(user_id: int, mode: str, question: str):
    now = datetime.now().strftime("%d.%m %H:%M")
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute(
            "INSERT INTO history (user_id,mode,question,created_at) VALUES (?,?,?,?)",
            (user_id, mode, question[:200], now)
        )
        await db.execute("""
            DELETE FROM history WHERE id NOT IN (
                SELECT id FROM history WHERE user_id=? ORDER BY id DESC LIMIT 20
            ) AND user_id=?
        """, (user_id, user_id))
        await db.commit()


async def get_history(user_id: int) -> list:
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        async with db.execute(
            "SELECT mode,question,created_at FROM history WHERE user_id=? ORDER BY id DESC LIMIT 5",
            (user_id,)
        ) as cur:
            return await cur.fetchall()

# ══════════════════════════════════════════════════════════════
# AI
# ══════════════════════════════════════════════════════════════

async def ask_ai(question: str, context: list = None, mode: str = "chat",
                 custom_system: str = None) -> str:
    if not OPENAI_KEY:
        return "❌ AI не настроен. Обратитесь к администратору."
    headers = {"Authorization": f"Bearer {OPENAI_KEY}", "Content-Type": "application/json"}

    system   = custom_system or SYSTEM_PROMPTS.get(mode, SYSTEM_PROMPTS["chat"])
    messages = [{"role": "system", "content": system}]
    if context and mode in ("chat", "hardcore", "praise"):
        messages.extend(context)
    messages.append({"role": "user", "content": question})

    # Спецформаты — чуть больше токенов и выше температура
    is_spec  = mode.startswith("spec_") or custom_system is not None
    payload  = {
        "model":       "gpt-4o",
        "messages":    messages,
        "max_tokens":  2000 if is_spec else 1500,
        "temperature": 0.9 if is_spec else 0.7,
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
        f"⚡ День — {PRICE_DAY} Stars\n"
        f"🔥 Неделя — {PRICE_WEEK} Stars\n"
        f"💎 Месяц — {PRICE_MONTH} Stars\n\n"
        f"Или пригласи друга — получи +20 бонусных вопросов!\n\n"
        f"_Завтра лимит обновится автоматически._"
    )

# ══════════════════════════════════════════════════════════════
# БОТ + ДИСПЕТЧЕР
# ══════════════════════════════════════════════════════════════

bot = Bot(token=BOT_TOKEN)
dp  = Dispatcher(storage=MemoryStorage())

MENU_BUTTONS = {
    "😈🗡️ Злой спорщик", "👑 Императорский",
    "🧠 Психолог", "🔮 Гороскоп",
    "💬 ChatGPT", "🌍 Переводчик", "✍️ Редактор текста",
    "💎 Premium", "📋 История", "👤 Профиль",
    "🚀 Поделиться", "📁 Проекты", "❓ Помощь",
    "💬 Новый диалог",
}

# ══════════════════════════════════════════════════════════════
# /start
# ══════════════════════════════════════════════════════════════

@dp.message(CommandStart())
async def cmd_start(msg: Message, state: FSMContext):
    await state.clear()
    fname = msg.from_user.first_name or ""
    user  = await get_user(msg.from_user.id, msg.from_user.username or "", fname)
    args  = msg.text.split()

    # Реферал — только если пользователь НОВЫЙ (onboarding_done=0) и ref_from не установлен
    # ЗАЩИТА: повторный /start не даёт бонусы и не сбрасывает счётчики
    if (len(args) > 1
            and args[1].isdigit()
            and int(args[1]) != msg.from_user.id
            and not user.get("ref_from", 0)
            and not user.get("onboarding_done", 0)):  # ← только для новых!
        ref_id = int(args[1])
        await _db("UPDATE users SET ref_from=? WHERE user_id=?", (ref_id, msg.from_user.id))
        await _db("UPDATE users SET invited_count=invited_count+1 WHERE user_id=?", (ref_id,))
        await _db("UPDATE users SET bonus_q=bonus_q+20 WHERE user_id=?", (ref_id,))
        await _db("UPDATE users SET bonus_q=bonus_q+10 WHERE user_id=?", (msg.from_user.id,))
        try:
            await bot.send_message(
                ref_id,
                "🚀 *По твоей ссылке пришёл новый пользователь!*\n\n+20 бонусных вопросов зачислено 🎁",
                parse_mode="Markdown"
            )
        except Exception:
            pass

    if not user.get("onboarding_done", 0):
        await state.set_state(Onboarding.waiting_first)
        return await msg.answer(
            ONBOARDING_1.format(name=fname or "друг", free_limit=FREE_LIMIT),
            parse_mode="Markdown",
            reply_markup=main_kb()
        )

    # Существующий пользователь — просто показываем меню, НИЧЕГО не сбрасываем
    await msg.answer(
        WELCOME_BACK.format(name=fname or "друг", left=questions_left(user)),
        parse_mode="Markdown",
        reply_markup=main_kb()
    )

# ══════════════════════════════════════════════════════════════
# ОНБОРДИНГ
# ══════════════════════════════════════════════════════════════

@dp.message(Onboarding.waiting_first)
async def onboarding_first(msg: Message, state: FSMContext):
    await state.clear()
    await _db("UPDATE users SET onboarding_done=1 WHERE user_id=?", (msg.from_user.id,))
    user = await get_user(msg.from_user.id)
    ok, reason = can_ask(user)
    if not ok:
        return await msg.answer(limit_text(), parse_mode="Markdown", reply_markup=limit_kb())

    wait    = await msg.answer(ONBOARDING_2, parse_mode="Markdown")
    await asyncio.sleep(1)
    context = []
    answer  = await ask_ai(msg.text, context, mode="chat")
    await save_question_used(msg.from_user.id, reason == "bonus")
    await touch_active(msg.from_user.id)
    await save_history(msg.from_user.id, "chat", msg.text)
    context.extend([
        {"role": "user",      "content": msg.text},
        {"role": "assistant", "content": answer},
    ])
    await save_context(msg.from_user.id, "chat", context)
    await bot.delete_message(msg.chat.id, wait.message_id)

    user   = await get_user(msg.from_user.id)
    left   = questions_left(user)
    footer = f"\n\n_💬 Осталось вопросов сегодня: {left}_"
    kb     = after_answer_premium_kb() if user["is_premium"] else after_answer_kb()
    await msg.answer(answer + footer, parse_mode="Markdown", reply_markup=kb)

# ══════════════════════════════════════════════════════════════
# КОМАНДЫ
# ══════════════════════════════════════════════════════════════

@dp.message(Command("premium"))
async def cmd_premium(msg: Message):
    await msg.answer(PREMIUM_TEXT, parse_mode="Markdown", reply_markup=premium_kb())

@dp.message(Command("profile"))
async def cmd_profile(msg: Message):
    user = await get_user(msg.from_user.id, msg.from_user.username or "")
    left = questions_left(user)
    prem = f"✅ активен до {user['premium_until']}" if user["is_premium"] else "❌ не активен"
    mode_name = MODES.get(user.get("mode", "chat"), MODES["chat"])["name"]
    hc_left = max(0, HARDCORE_FREE - (user.get("hardcore_free_used") or 0))
    pr_left = max(0, PRAISE_FREE   - (user.get("praise_free_used")   or 0))
    await msg.answer(
        f"👤 *Твой аккаунт*\n\n"
        f"Активный режим: {mode_name}\n\n"
        f"Вопросов сегодня: *{user['questions_today']}/{FREE_LIMIT if not user['is_premium'] else 100}*\n"
        f"Бонусных вопросов: *{user['bonus_q']}*\n"
        f"♻️ Лимит обновляется каждые 24 часа\n\n"
        f"🗡️ Бесплатных дуэлей осталось: *{hc_left if not user['is_premium'] else '∞'}*\n"
        f"👑 Бесплатных аудиенций осталось: *{pr_left if not user['is_premium'] else '∞'}*\n\n"
        f"Друзей приглашено: *{user['invited_count']}*\n"
        f"Всего вопросов: *{user['total_questions']}*\n\n"
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
            ref_link=ref_link, bonus_inviter=20, bonus_invited=10,
            invited=user["invited_count"], bonus_q=user["bonus_q"],
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
        "⚖️ *Мой Юрист*\nЮридический помощник по российскому праву.\n\n"
        "🤖 *ChatGPT Free* (здесь)\nAI-помощник на базе GPT-4o. 5 режимов общения.\n\n"
        "_Больше проектов скоро..._",
        parse_mode="Markdown",
        reply_markup=InlineKeyboardMarkup(inline_keyboard=[
            [InlineKeyboardButton(text="⚖️ Мой Юрист — перейти", url=LAWYER_BOT_URL)],
        ])
    )

@dp.message(Command("clear"))
async def cmd_clear(msg: Message):
    await clear_context(msg.from_user.id)
    await msg.answer("🗑 *История диалога очищена*\n\nНачинаем с чистого листа!", parse_mode="Markdown")

@dp.message(Command("history"))
async def cmd_history(msg: Message):
    rows = await get_history(msg.from_user.id)
    if not rows:
        return await msg.answer("📋 *История пуста*\n\nЗадай первый вопрос!", parse_mode="Markdown")
    icons = {"chat": "💬", "translate": "🌍", "editor": "✍️",
             "hardcore": "🗡️", "praise": "👑", "spec_ode": "👑",
             "spec_battle": "🔥", "spec_parody": "☠️", "spec_panegyric": "📜"}
    lines = []
    for i, row in enumerate(rows, 1):
        icon = icons.get(row["mode"], "💬")
        q    = row["question"][:60] + ("..." if len(row["question"]) > 60 else "")
        lines.append(f"{i}. {icon} _{row['created_at']}_\n`{q}`")
    await msg.answer("📋 *Последние запросы:*\n\n" + "\n\n".join(lines), parse_mode="Markdown")

@dp.message(Command("help"))
async def cmd_help(msg: Message):
    await msg.answer(
        HELP_TEXT.format(free_limit=FREE_LIMIT),
        parse_mode="Markdown",
        reply_markup=InlineKeyboardMarkup(inline_keyboard=[
            [InlineKeyboardButton(text="🎭 Режимы бота",          callback_data="modes_screen")],
            [InlineKeyboardButton(text="💎 Преимущества Premium", callback_data="show_premium")],
            [InlineKeyboardButton(text="🚀 Реферальная система",  callback_data="share")],
        ])
    )

# ══════════════════════════════════════════════════════════════
# ADMIN
# ══════════════════════════════════════════════════════════════

@dp.message(Command("admin"))
async def cmd_admin(msg: Message):
    if msg.from_user.id not in ADMIN_IDS: return
    total, premium, questions = await get_stats()
    await msg.answer(
        f"🔧 *Панель администратора*\n\n"
        f"👥 Пользователей: {total}\n"
        f"💎 Premium: {premium}\n"
        f"❓ Вопросов: {questions}\n\n"
        f"`/premium_add ID [дни]`\n"
        f"`/premium_remove ID`\n"
        f"`/broadcast текст`\n"
        f"`/stats`",
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

# ══════════════════════════════════════════════════════════════
# КНОПКИ НИЖНЕГО МЕНЮ
# ══════════════════════════════════════════════════════════════

@dp.message(F.text == "💬 Новый диалог")
async def handle_new_dialog(msg: Message):
    await clear_context(msg.from_user.id)
    await msg.answer("💬 *Новый диалог начат!*\n\nИстория очищена 👇", parse_mode="Markdown")

@dp.message(F.text == "💬 ChatGPT")
async def handle_mode_chat(msg: Message):
    await set_mode(msg.from_user.id, "chat")
    await msg.answer(
        "💬 *Режим: ChatGPT*\n\nЗадавай любые вопросы — отвечу развёрнуто и по делу.\n\n"
        "_История диалога сброшена._",
        parse_mode="Markdown", reply_markup=main_kb()
    )

@dp.message(F.text == "🌍 Переводчик")
async def handle_mode_translate(msg: Message):
    await set_mode(msg.from_user.id, "translate")
    await msg.answer(
        "🌍 *Режим: Переводчик*\n\nОтправь текст — переведу автоматически:\n"
        "• Русский → Английский\n• Любой язык → Русский\n\nПросто отправь текст 👇",
        parse_mode="Markdown", reply_markup=main_kb()
    )

@dp.message(F.text == "✍️ Редактор текста")
async def handle_mode_editor(msg: Message):
    await set_mode(msg.from_user.id, "editor")
    await msg.answer(
        "✍️ *Режим: Редактор текста*\n\nОтправь любой текст — исправлю ошибки, "
        "улучшу стиль и объясню изменения.\n\nПодходит для писем, постов, резюме 👇",
        parse_mode="Markdown", reply_markup=main_kb()
    )

@dp.message(F.text == "🧠 Психолог")
async def handle_psycho_btn(msg: Message):
    user = await get_user(msg.from_user.id)
    used = user.get("psycho_free_used", 0)
    if not user["is_premium"] and used >= PSYCHO_FREE:
        return await msg.answer(PSYCHO_PAYWALL, parse_mode="Markdown", reply_markup=psycho_paywall_kb())
    await set_mode(msg.from_user.id, "psycho")
    await msg.answer(
        PSYCHO_ONBOARD.format(free=PSYCHO_FREE - used),
        parse_mode="Markdown", reply_markup=main_kb()
    )

@dp.message(F.text == "🔮 Гороскоп")
async def handle_horoscope_btn(msg: Message):
    user = await get_user(msg.from_user.id)
    used = user.get("horoscope_free_used", 0)
    if not user["is_premium"] and used >= HOROSCOPE_FREE:
        return await msg.answer(HOROSCOPE_PAYWALL, parse_mode="Markdown", reply_markup=horoscope_paywall_kb())
    await set_mode(msg.from_user.id, "horoscope")
    await msg.answer(
        HOROSCOPE_ONBOARD.format(free=HOROSCOPE_FREE - used),
        parse_mode="Markdown", reply_markup=main_kb()
    )

@dp.message(F.text == "😈🗡️ Злой спорщик")
async def handle_hardcore_btn(msg: Message):
    await msg.answer(HARDCORE_HOOK, parse_mode="Markdown", reply_markup=hardcore_hook_kb())

@dp.message(F.text == "👑 Императорский")
async def handle_praise_btn(msg: Message):
    await msg.answer(PRAISE_HOOK, parse_mode="Markdown", reply_markup=praise_hook_kb())

@dp.message(F.text == "💎 Premium")
async def handle_premium_btn(msg: Message):
    await msg.answer(PREMIUM_TEXT, parse_mode="Markdown", reply_markup=premium_kb())

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
    await cmd_projects(msg)

@dp.message(F.text == "❓ Помощь")
async def handle_help_btn(msg: Message):
    await cmd_help(msg)

# ══════════════════════════════════════════════════════════════
# CALLBACKS
# ══════════════════════════════════════════════════════════════

@dp.callback_query(F.data == "show_premium")
async def cb_show_premium(cb: CallbackQuery):
    await cb.answer()
    await cb.message.answer(PREMIUM_TEXT, parse_mode="Markdown", reply_markup=premium_kb())

@dp.callback_query(F.data == "modes_screen")
async def cb_modes_screen(cb: CallbackQuery):
    await cb.answer()
    await cb.message.answer(MODES_SCREEN_TEXT, parse_mode="Markdown", reply_markup=modes_screen_kb())

@dp.callback_query(F.data == "special_formats")
async def cb_special_formats(cb: CallbackQuery):
    await cb.answer()
    user = await get_user(cb.from_user.id)
    if not user["is_premium"]:
        return await cb.message.answer(
            SPECIAL_FORMATS_TEXT + "\n\n_💎 Спецформаты доступны в Premium_",
            parse_mode="Markdown", reply_markup=premium_with_demo_kb()
        )
    await cb.message.answer(SPECIAL_FORMATS_TEXT, parse_mode="Markdown", reply_markup=special_formats_kb())

@dp.callback_query(F.data == "demo_screen")
async def cb_demo_screen(cb: CallbackQuery):
    await cb.answer()
    await cb.message.answer(DEMO_SCREEN_TEXT, parse_mode="Markdown", reply_markup=demo_screen_kb())

@dp.callback_query(F.data == "demo_battle")
async def cb_demo_battle(cb: CallbackQuery):
    await cb.answer()
    user = await get_user(cb.from_user.id)
    if user["is_premium"]:
        await set_mode(cb.from_user.id, "spec_battle")
        return await cb.message.answer(
            "🔥 *Режим: Баттл-стих*\n\nНапиши тезис или аргумент — разнесу в стихах 🎤",
            parse_mode="Markdown"
        )
    await set_mode(cb.from_user.id, "demo_battle")
    await cb.message.answer(
        "🔥 *Демо: Баттл-стих*\n\n"
        "Напиши любой тезис или утверждение.\n"
        "Одна бесплатная стихотворная атака 🎤\n\n"
        "_После — Premium для продолжения._",
        parse_mode="Markdown"
    )

@dp.callback_query(F.data.startswith("spec:"))
async def cb_special_format(cb: CallbackQuery):
    await cb.answer()
    user = await get_user(cb.from_user.id)
    if not user["is_premium"]:
        return await cb.message.answer(
            SPECIAL_FORMATS_TEXT + "\n\n_💎 Спецформаты доступны в Premium_",
            parse_mode="Markdown", reply_markup=premium_with_demo_kb()
        )
    fmt = cb.data.split(":")[1]
    labels = {
        "ode":       ("👑", "Царская ода",      "Напиши тему — получишь торжественную оду 📜"),
        "battle":    ("🔥", "Баттл-стих",       "Напиши тезис — разнесу в рифмах 🎤"),
        "parody":    ("☠️", "Ядовитая пародия", "Напиши аргумент — сделаю сатирический демонтаж 🎭"),
        "panegyric": ("📜", "Панегирик",         "Напиши о себе или теме — создам литературный пьедестал 🏛️"),
    }
    emoji, name, hint = labels.get(fmt, ("🎭", "Спецформат", "Напиши запрос 👇"))
    await cb.message.answer(f"{emoji} *Режим: {name}*\n\n{hint}", parse_mode="Markdown")
    await set_mode(cb.from_user.id, f"spec_{fmt}")

@dp.callback_query(F.data == "zone_psycho")
async def cb_enter_psycho(cb: CallbackQuery):
    await cb.answer()
    user = await get_user(cb.from_user.id)
    used = user.get("psycho_free_used", 0)
    if not user["is_premium"] and used >= PSYCHO_FREE:
        return await cb.message.answer(PSYCHO_PAYWALL, parse_mode="Markdown", reply_markup=psycho_paywall_kb())
    await set_mode(cb.from_user.id, "psycho")
    await cb.message.answer(
        PSYCHO_ONBOARD.format(free=PSYCHO_FREE - used),
        parse_mode="Markdown", reply_markup=main_kb()
    )

@dp.callback_query(F.data == "zone_horoscope")
async def cb_enter_horoscope(cb: CallbackQuery):
    await cb.answer()
    user = await get_user(cb.from_user.id)
    used = user.get("horoscope_free_used", 0)
    if not user["is_premium"] and used >= HOROSCOPE_FREE:
        return await cb.message.answer(HOROSCOPE_PAYWALL, parse_mode="Markdown", reply_markup=horoscope_paywall_kb())
    await set_mode(cb.from_user.id, "horoscope")
    await cb.message.answer(
        HOROSCOPE_ONBOARD.format(free=HOROSCOPE_FREE - used),
        parse_mode="Markdown", reply_markup=main_kb()
    )

@dp.callback_query(F.data == "zone_hardcore")
async def cb_enter_hardcore(cb: CallbackQuery):
    await cb.answer()
    user    = await get_user(cb.from_user.id)
    hc_used = user.get("hardcore_free_used", 0)
    if not user["is_premium"] and hc_used >= HARDCORE_FREE:
        return await cb.message.answer(
            HARDCORE_PAYWALL, parse_mode="Markdown", reply_markup=hardcore_paywall_kb()
        )
    await set_mode(cb.from_user.id, "hardcore")
    await cb.message.answer(HARDCORE_ONBOARD, parse_mode="Markdown", reply_markup=main_kb())

@dp.callback_query(F.data == "zone_hardcore_info")
async def cb_hardcore_info(cb: CallbackQuery):
    await cb.answer()
    await cb.message.answer(DANGER_ZONE_TEXT, parse_mode="Markdown", reply_markup=danger_zone_kb())

@dp.callback_query(F.data == "zone_praise")
async def cb_enter_praise(cb: CallbackQuery):
    await cb.answer()
    user    = await get_user(cb.from_user.id)
    pr_used = user.get("praise_free_used", 0)
    if not user["is_premium"] and pr_used >= PRAISE_FREE:
        return await cb.message.answer(
            PRAISE_PAYWALL, parse_mode="Markdown", reply_markup=praise_paywall_kb()
        )
    await set_mode(cb.from_user.id, "praise")
    await cb.message.answer(PRAISE_ONBOARD, parse_mode="Markdown", reply_markup=main_kb())

@dp.callback_query(F.data == "zone_praise_info")
async def cb_praise_info(cb: CallbackQuery):
    await cb.answer()
    await cb.message.answer(PRAISE_HOOK, parse_mode="Markdown", reply_markup=praise_hook_kb())

@dp.callback_query(F.data == "share")
async def cb_share(cb: CallbackQuery):
    await cb.answer()
    bot_info = await bot.get_me()
    uid      = cb.from_user.id
    ref_link = f"https://t.me/{bot_info.username}?start={uid}"
    user     = await get_user(uid)
    await cb.message.answer(
        SHARE_TEXT.format(
            ref_link=ref_link, bonus_inviter=20, bonus_invited=10,
            invited=user["invited_count"], bonus_q=user["bonus_q"],
        ),
        parse_mode="Markdown",
        reply_markup=InlineKeyboardMarkup(inline_keyboard=[
            [InlineKeyboardButton(
                text="📤 Поделиться",
                url=f"https://t.me/share/url?url={ref_link}&text=Попробуй+этого+AI-помощника+🤖"
            )],
        ])
    )

@dp.callback_query(F.data == "continue")
async def cb_continue(cb: CallbackQuery):
    await cb.answer()
    await cb.message.answer("💬 Продолжаем! Задавай следующий вопрос 👇")

@dp.callback_query(F.data == "features")
async def cb_features(cb: CallbackQuery):
    await cb.answer()
    await cb.message.answer(MODES_SCREEN_TEXT, parse_mode="Markdown", reply_markup=modes_screen_kb())

# ── Оплата ───────────────────────────────────────────────────

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
            f"🗡️ Жёсткий спорщик — без лимитов\n"
            f"👑 Императорский подхалим — без лимитов\n"
            f"🔥 Баттл-стихи, пародии, оды\n"
            f"💬 ChatGPT — 100 вопросов в сутки\n"
            f"📅 {desc_days}"
        ),
        payload=payload,
        currency="XTR",
        prices=[LabeledPrice(label=f"Premium {desc_days}", amount=amount)],
    )

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
        f"Теперь тебе доступны все режимы без лимитов:\n"
        f"🗡️ Жёсткий спорщик\n"
        f"👑 Императорский подхалим\n"
        f"🔥 Баттл-стихи, пародии, оды\n"
        f"💬 ChatGPT — 100 вопросов/день\n\n"
        f"Задавай — я готов! 🤖",
        parse_mode="Markdown", reply_markup=main_kb()
    )
    for admin_id in ADMIN_IDS:
        try:
            await bot.send_message(
                admin_id,
                f"💰 Новая оплата!\n@{msg.from_user.username or msg.from_user.id}\n"
                f"⭐ {stars} Stars / {period}"
            )
        except Exception:
            pass

# ══════════════════════════════════════════════════════════════
# ГЛАВНЫЙ ОБРАБОТЧИК СООБЩЕНИЙ
# ══════════════════════════════════════════════════════════════

@dp.message(F.text & ~F.text.startswith("/"))
async def handle_message(msg: Message, state: FSMContext):
    if await state.get_state(): return
    if msg.text.strip() in MENU_BUTTONS: return

    user = await get_user(msg.from_user.id, msg.from_user.username or "")
    text = msg.text.strip()

    if len(text) > MAX_MSG_LEN:
        return await msg.answer(
            f"⚠️ *Слишком длинный текст*\n\nМаксимум — {MAX_MSG_LEN} символов.\n"
            f"Разбей на части 👇", parse_mode="Markdown"
        )
    if len(text) < 2:
        return await msg.answer("💬 Напиши вопрос или текст — я отвечу!", reply_markup=main_kb())

    mode = await get_mode(msg.from_user.id)

    # ── HARDCORE ────────────────────────────────────────────
    if mode == "hardcore":
        hc_used = user.get("hardcore_free_used", 0)
        if not user["is_premium"] and hc_used >= HARDCORE_FREE:
            return await msg.answer(HARDCORE_PAYWALL, parse_mode="Markdown", reply_markup=hardcore_paywall_kb())

        wait_sec = await check_cooldown(msg.from_user.id)
        if wait_sec > 0:
            return await msg.answer(f"⏳ Подожди {int(wait_sec)} сек.")

        context = await get_context(msg.from_user.id, "hardcore")
        wait    = await msg.answer("🗡️ _Формирую удар..._", parse_mode="Markdown")
        answer  = await ask_ai(text, context, mode="hardcore")
        context.extend([{"role": "user", "content": text}, {"role": "assistant", "content": answer}])
        await save_context(msg.from_user.id, "hardcore", context)
        await save_history(msg.from_user.id, "hardcore", text)
        await touch_active(msg.from_user.id)
        await bot.delete_message(msg.chat.id, wait.message_id)

        if not user["is_premium"]:
            await _db("UPDATE users SET hardcore_free_used=hardcore_free_used+1 WHERE user_id=?", (msg.from_user.id,))
            if hc_used + 1 >= HARDCORE_FREE:
                await msg.answer(answer, parse_mode="Markdown")
                return await msg.answer(
                    "🗡️ *Бесплатная дуэль завершена.*\n\n_Настоящая дуэль длится дольше одного удара._",
                    parse_mode="Markdown", reply_markup=after_hardcore_free_kb()
                )

        left_hc = "∞" if user["is_premium"] else str(HARDCORE_FREE - hc_used - 1)
        footer  = f"\n\n_🗡️ Жёсткий спорщик · {'∞' if user['is_premium'] else f'{left_hc} дуэлей бесплатно'}_"
        kb      = after_answer_premium_kb() if user["is_premium"] else None
        return await msg.answer(answer + footer, parse_mode="Markdown", reply_markup=kb)

    # ── PRAISE ──────────────────────────────────────────────
    if mode == "praise":
        pr_used = user.get("praise_free_used", 0)
        if not user["is_premium"] and pr_used >= PRAISE_FREE:
            return await msg.answer(PRAISE_PAYWALL, parse_mode="Markdown", reply_markup=praise_paywall_kb())

        wait_sec = await check_cooldown(msg.from_user.id)
        if wait_sec > 0:
            return await msg.answer(f"⏳ Подожди {int(wait_sec)} сек.")

        context = await get_context(msg.from_user.id, "praise")
        wait    = await msg.answer("👑 _Готовлю церемонию..._", parse_mode="Markdown")
        answer  = await ask_ai(text, context, mode="praise")
        context.extend([{"role": "user", "content": text}, {"role": "assistant", "content": answer}])
        await save_context(msg.from_user.id, "praise", context)
        await save_history(msg.from_user.id, "praise", text)
        await touch_active(msg.from_user.id)
        await bot.delete_message(msg.chat.id, wait.message_id)

        if not user["is_premium"]:
            await _db("UPDATE users SET praise_free_used=praise_free_used+1 WHERE user_id=?", (msg.from_user.id,))
            if pr_used + 1 >= PRAISE_FREE:
                await msg.answer(answer, parse_mode="Markdown")
                return await msg.answer(
                    "👑 *Бесплатная аудиенция завершена.*\n\n_Твоё величие заслуживает большего._",
                    parse_mode="Markdown", reply_markup=after_praise_free_kb()
                )

        footer = f"\n\n_👑 Императорский · {'∞' if user['is_premium'] else '0 аудиенций бесплатно'}_"
        kb     = after_answer_premium_kb() if user["is_premium"] else None
        return await msg.answer(answer + footer, parse_mode="Markdown", reply_markup=kb)

    # ── ПСИХОЛОГ ────────────────────────────────────────────
    if mode == "psycho":
        used = user.get("psycho_free_used", 0)
        if not user["is_premium"] and used >= PSYCHO_FREE:
            return await msg.answer(PSYCHO_PAYWALL, parse_mode="Markdown", reply_markup=psycho_paywall_kb())

        wait_sec = await check_cooldown(msg.from_user.id)
        if wait_sec > 0:
            return await msg.answer(f"⏳ Подожди {int(wait_sec)} сек.")

        context = await get_context(msg.from_user.id, "psycho")
        wait    = await msg.answer("🧠 _Слушаю вас..._", parse_mode="Markdown")
        answer  = await ask_ai(text, context, mode="psycho")
        context.extend([{"role": "user", "content": text}, {"role": "assistant", "content": answer}])
        await save_context(msg.from_user.id, "psycho", context)
        await save_history(msg.from_user.id, "psycho", text)
        await touch_active(msg.from_user.id)
        await bot.delete_message(msg.chat.id, wait.message_id)

        if not user["is_premium"]:
            await _db("UPDATE users SET psycho_free_used=psycho_free_used+1 WHERE user_id=?", (msg.from_user.id,))
            new_used = used + 1
            if new_used >= PSYCHO_FREE:
                await msg.answer(answer, parse_mode="Markdown")
                return await msg.answer(PSYCHO_PAYWALL, parse_mode="Markdown", reply_markup=psycho_paywall_kb())

        left   = PSYCHO_FREE - used - 1 if not user["is_premium"] else "∞"
        footer = f"\n\n_🧠 Психолог · {'∞' if user['is_premium'] else f'{left} сессий бесплатно'}_"
        kb     = after_answer_premium_kb() if user["is_premium"] else None
        return await msg.answer(answer + footer, parse_mode="Markdown", reply_markup=kb)

    # ── ГОРОСКОП ────────────────────────────────────────────
    if mode == "horoscope":
        used = user.get("horoscope_free_used", 0)
        if not user["is_premium"] and used >= HOROSCOPE_FREE:
            return await msg.answer(HOROSCOPE_PAYWALL, parse_mode="Markdown", reply_markup=horoscope_paywall_kb())

        wait_sec = await check_cooldown(msg.from_user.id)
        if wait_sec > 0:
            return await msg.answer(f"⏳ Подожди {int(wait_sec)} сек.")

        context = await get_context(msg.from_user.id, "horoscope")
        wait    = await msg.answer("🔮 _Читаю звёзды..._", parse_mode="Markdown")
        answer  = await ask_ai(text, context, mode="horoscope")
        context.extend([{"role": "user", "content": text}, {"role": "assistant", "content": answer}])
        await save_context(msg.from_user.id, "horoscope", context)
        await save_history(msg.from_user.id, "horoscope", text)
        await touch_active(msg.from_user.id)
        await bot.delete_message(msg.chat.id, wait.message_id)

        if not user["is_premium"]:
            await _db("UPDATE users SET horoscope_free_used=horoscope_free_used+1 WHERE user_id=?", (msg.from_user.id,))
            new_used = used + 1
            if new_used >= HOROSCOPE_FREE:
                await msg.answer(answer, parse_mode="Markdown")
                return await msg.answer(HOROSCOPE_PAYWALL, parse_mode="Markdown", reply_markup=horoscope_paywall_kb())

        left   = HOROSCOPE_FREE - used - 1 if not user["is_premium"] else "∞"
        footer = f"\n\n_🔮 Гороскоп · {'∞' if user['is_premium'] else f'{left} запроса бесплатно'}_"
        kb     = after_answer_premium_kb() if user["is_premium"] else None
        return await msg.answer(answer + footer, parse_mode="Markdown", reply_markup=kb)

    # ── СПЕЦФОРМАТЫ ─────────────────────────────────────────
    if mode.startswith("spec_") or mode == "demo_battle":
        fmt_key = mode.replace("spec_", "") if mode.startswith("spec_") else "battle"
        is_demo = mode == "demo_battle"

        if is_demo and not user["is_premium"]:
            hc_used = user.get("hardcore_free_used", 0)
            if hc_used >= HARDCORE_FREE:
                await set_mode(msg.from_user.id, "chat")
                return await msg.answer(HARDCORE_PAYWALL, parse_mode="Markdown", reply_markup=hardcore_paywall_kb())

        if not is_demo and not user["is_premium"]:
            await set_mode(msg.from_user.id, "chat")
            return await msg.answer(
                SPECIAL_FORMATS_TEXT + "\n\n_💎 Спецформаты доступны в Premium_",
                parse_mode="Markdown", reply_markup=premium_with_demo_kb()
            )

        icons = {"ode": "👑", "battle": "🔥", "parody": "☠️", "panegyric": "📜"}
        icon  = icons.get(fmt_key, "🎭")
        wait  = await msg.answer(f"{icon} _Готовлю спецформат..._", parse_mode="Markdown")

        spec_system = SPECIAL_PROMPTS.get(fmt_key, SPECIAL_PROMPTS["battle"])
        answer = await ask_ai(text, [], mode="chat", custom_system=spec_system)

        await save_history(msg.from_user.id, mode, text)
        await touch_active(msg.from_user.id)
        await bot.delete_message(msg.chat.id, wait.message_id)

        mode_info = MODES.get(mode, {"name": "Спецформат", "emoji": icon})
        footer    = f"\n\n_{icon} {mode_info['name']}_"

        if is_demo and not user["is_premium"]:
            await _db("UPDATE users SET hardcore_free_used=hardcore_free_used+1 WHERE user_id=?", (msg.from_user.id,))
            await msg.answer(answer + footer, parse_mode="Markdown")
            await set_mode(msg.from_user.id, "chat")
            return await msg.answer(
                "🔥 *Демо завершено.*\n\n_Хочешь ещё? Открой Premium — все 4 спецформата без лимитов._",
                parse_mode="Markdown", reply_markup=premium_with_demo_kb()
            )

        await msg.answer(answer + footer, parse_mode="Markdown",
            reply_markup=InlineKeyboardMarkup(inline_keyboard=[
                [InlineKeyboardButton(text="🎭 Другой спецформат",  callback_data="special_formats")],
                [InlineKeyboardButton(text="💬 Вернуться в ChatGPT", callback_data="continue")],
            ])
        )
        await set_mode(msg.from_user.id, "chat")
        return

    # ── СТАНДАРТНЫЕ РЕЖИМЫ ───────────────────────────────────
    ok, reason = can_ask(user)
    if not ok:
        return await msg.answer(limit_text(), parse_mode="Markdown", reply_markup=limit_kb())

    wait_sec = await check_cooldown(msg.from_user.id)
    if wait_sec > 0:
        return await msg.answer(f"⏳ Подожди {int(wait_sec)} сек. перед следующим запросом.")

    mode_info  = MODES.get(mode, MODES["chat"])
    wait_texts = {
        "chat":      "🤖 _Думаю..._",
        "translate": "🌍 _Перевожу..._",
        "editor":    "✍️ _Редактирую..._",
    }

    context = await get_context(msg.from_user.id, mode) if mode == "chat" else []
    wait    = await msg.answer(wait_texts.get(mode, "🤖 _Обрабатываю..._"), parse_mode="Markdown")
    answer  = await ask_ai(text, context, mode=mode)

    await save_question_used(msg.from_user.id, reason == "bonus")
    await touch_active(msg.from_user.id)
    await save_history(msg.from_user.id, mode, text)

    if mode == "chat":
        context.extend([{"role": "user", "content": text}, {"role": "assistant", "content": answer}])
        await save_context(msg.from_user.id, "chat", context)

    await bot.delete_message(msg.chat.id, wait.message_id)

    user  = await get_user(msg.from_user.id)
    left  = questions_left(user)

    if user["is_premium"]:
        footer = f"\n\n_{mode_info['emoji']} {mode_info['name']} · {user['questions_today']}/100_"
        kb     = after_answer_premium_kb()
    else:
        footer = f"\n\n_{mode_info['emoji']} {mode_info['name']} · Осталось: {left} из {FREE_LIMIT}_"
        kb     = after_answer_kb() if int(left) <= 1 else None

    await msg.answer(answer + footer, parse_mode="Markdown", reply_markup=kb)

# ══════════════════════════════════════════════════════════════
# ФОНОВЫЕ ЗАДАЧИ
# ══════════════════════════════════════════════════════════════

async def send_reminders():
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
                            [InlineKeyboardButton(text="🗡️ Злой спорщик",  callback_data="zone_hardcore")],
                            [InlineKeyboardButton(text="👑 Императорский", callback_data="zone_praise")],
                            [InlineKeyboardButton(text="💬 ChatGPT",       callback_data="continue")],
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
    commands = [
        BotCommand(command="start",    description="🤖 Главное меню"),
        BotCommand(command="premium",  description="💎 Получить Premium"),
        BotCommand(command="profile",  description="👤 Твой аккаунт"),
        BotCommand(command="share",    description="🚀 Поделиться с другом"),
        BotCommand(command="projects", description="📁 Наши проекты"),
        BotCommand(command="history",  description="📋 История запросов"),
        BotCommand(command="clear",    description="🗑 Очистить историю диалога"),
        BotCommand(command="help",     description="❓ Справка"),
    ]
    await bot.set_my_commands(commands, scope=BotCommandScopeDefault())


async def main():
    await init_db()
    await set_commands()
    logger.info("🤖 ChatGPT Free v2.0 запущен!")
    asyncio.create_task(send_reminders())
    await dp.start_polling(bot, skip_updates=True)


if __name__ == "__main__":
    asyncio.run(main())
