#include "TextBook.h"
#include "types.h"
#include <regex>


wchar_t TextBook::m_ValidChapter[] =
{
    _T(' '), _T('\t'),
    _T('0'), _T('1'), _T('2'), _T('3'), _T('4'),
    _T('5'), _T('6'), _T('7'), _T('8'), _T('9'),
    _T('零'), _T('一'), _T('二'), _T('三'), _T('四'),
    _T('五'), _T('六'), _T('七'), _T('八'), _T('九'),
    _T('十'), _T('百'), _T('千'), _T('万'), _T('亿'),
    _T('壹'), _T('贰'), _T('叁'), _T('肆'),
    _T('伍'), _T('陆'), _T('柒'), _T('捌'), _T('玖'),
    _T('拾'), _T('佰'), _T('仟'), _T('萬'), _T('億'),
    _T('两'), _T('〇'),
    0xFF10, 0xFF11, 0xFF12, 0xFF13, 0xFF14,
    0xFF15, 0xFF16, 0xFF17, 0xFF18, 0xFF19,
    0x3000, 0xA0
};

static BOOL IsChapterUnit(wchar_t ch)
{
    return ch == _T('卷') || ch == _T('章') || ch == _T('部') || ch == _T('节') || ch == _T('節')
        || ch == _T('回') || ch == _T('篇') || ch == _T('集') || ch == _T('册')
        || ch == _T('幕') || ch == _T('编') || ch == _T('话') || ch == _T('話')
        || ch == _T('讲') || ch == _T('講');
}

TextBook::TextBook()
{
}

TextBook::~TextBook()
{
    ForceKill();
}

book_type_t TextBook::GetBookType(void)
{
    return book_text;
}

BOOL TextBook::SaveBook(HWND hWnd)
{
    FILE *fp = NULL;

    fp = _tfopen(m_fileName, _T("wb"));
    if (!fp)
        return FALSE;

    fwrite("\xff\xfe", 2, 1, fp);
    fwrite(m_Text, sizeof(wchar_t), m_Length, fp);
    fclose(fp);

    return TRUE;
}

BOOL TextBook::UpdateChapters(int offset)
{
    chapters_t::iterator itor;
    
    for (itor=m_Chapters.begin(); itor!=m_Chapters.end(); itor++)
    {
        if (itor->index > m_Index)
        {
            itor->index += offset;
            if (itor->index < 0)
                itor->index = 0;
        }
    }
    return TRUE;
}

BOOL TextBook::ParserBook(HWND hWnd)
{
    BOOL ret = FALSE;

    if (!ReadBook())
        goto end;

    if (!ParserChapters())
        goto end;

    ret = TRUE;

end:
    if (!ret)
        CloseBook();

    return ret;
}

BOOL TextBook::ReadBook(void)
{
    FILE *fp = NULL;
    char *buf = NULL;
    int len;
    int size;
    BOOL ret = FALSE;

    if (m_Data && m_Size > 0)
    {
        buf = m_Data;
        len = m_Size;
    }
    else if (m_fileName[0])
    {
        fp = _tfopen(m_fileName, _T("rb"));
        if (!fp)
            goto end;

        fseek(fp, 0, SEEK_END);
        len = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        buf = (char *)malloc(len + 2);
        buf[len] = 0;
        buf[len+1] = 0;
        if (!buf)
            goto end;
        size = (int)fread(buf, 1, len, fp);
        if (size != len)
            goto end;
    }
    else
    {
        goto end;
    }

    if (!DecodeText(buf, len, &m_Text, &m_Length))
        goto end;

    if (m_bForceKill)
        goto end;

    ret = TRUE;

end:
    if (fp)
        fclose(fp);
    if (buf)
        free(buf);
    if (m_Data)
        m_Data = NULL;
    m_Size = 0;

    return ret;
}

BOOL TextBook::ParserChapters(void)
{
    BOOL ret = FALSE;

    if (!m_Rule)
    {
        return ParserChaptersDefault();
    }

    m_Chapters.clear();

    // Always try the built-in detector first. It recognizes ordinary
    // 第...章 headings even when an old custom rule is still stored in
    // .cache.dat for another book.
    if (ParserChaptersDefault() && !m_Chapters.empty())
    {
        return TRUE;
    }

    m_Chapters.clear();
    if (m_Rule->rule == 1)
    {
        ret = ParserChaptersKeyword();
    }
    else if (m_Rule->rule == 2)
    {
        ret = ParserChaptersRegex();
    }

    // The chapter rule is stored globally in .cache.dat. If an old
    // custom keyword/regex does not match this book, do not leave the
    // chapter list empty: fall back to the built-in detector.
    if (m_Chapters.empty())
    {
        m_Chapters.clear();
        return ParserChaptersDefault();
    }

    return ret;
}

BOOL TextBook::ParserChaptersDefault(void)
{
    wchar_t *text = m_Text;
    wchar_t title[MAX_CHAPTER_LENGTH] = { 0 };
    int line_size;
    int title_len = 0;
    int idx_1 = -1;
    std::vector<int> chapter_starts;
    chapter_item_t chapter;

    while (TRUE)
    {
        if (m_bForceKill)
        {
            return FALSE;
        }

        if (!GetLine(text, m_Length - (int)(text - m_Text), &line_size, NULL, NULL, NULL, NULL))
        {
            break;
        }

        // Find every chapter marker on this line, not only the first one.
        chapter_starts.clear();
        idx_1 = -1;
        for (int i = 0; i < line_size; i++)
        {
            if (text[i] == _T('第'))
            {
                idx_1 = i;
                continue;
            }
            // Accept the chapter unit immediately after its number; the
            // title may follow without a space, colon, or other separator.
            if (idx_1 > -1 && IsChapterUnit(text[i])
                && IsChapter(text + idx_1 + 1, i - idx_1 - 1))
            {
                chapter_starts.push_back(idx_1);
                idx_1 = -1;
                continue;
            }
            if (idx_1 == -1 && i + 1 < line_size && text[i] == _T('楔') && text[i + 1] == _T('子')
                && (i + 2 == line_size || text[i + 2] == _T(' ') || text[i + 2] == _T('\t')
                    || text[i + 2] == 0x3000))
            {
                chapter_starts.push_back(i);
            }
            if (idx_1 == -1 && i + 1 < line_size && text[i] == _T('序') && text[i + 1] == _T('章')
                && (i + 2 == line_size || text[i + 2] == _T(' ') || text[i + 2] == _T('\t')
                    || text[i + 2] == 0x3000))
            {
                chapter_starts.push_back(i);
            }
        }
        for (size_t j = 0; j < chapter_starts.size(); j++)
        {
            int start = chapter_starts[j];
            int end = (j + 1 < chapter_starts.size()) ? chapter_starts[j + 1] : line_size;
            title_len = end - start < (MAX_CHAPTER_LENGTH - 1) ? end - start : MAX_CHAPTER_LENGTH - 1;
            memcpy(title, text + start, title_len * sizeof(wchar_t));
            title[title_len] = 0;

            // Start at the marker, not at the beginning of the line.
            chapter.index = (int)(text - m_Text) + start;
            chapter.title = title;
            chapter.title_len = title_len;
            m_Chapters.push_back(chapter);
        }

        // set index
        text += line_size + 1; // add 0x0a
    }

    return TRUE;
}

BOOL TextBook::ParserChaptersKeyword(void)
{
    wchar_t *text = m_Text;
    wchar_t title[MAX_CHAPTER_LENGTH] = { 0 };
    int line_size;
    int title_len = 0;
    BOOL bFound = FALSE;
    int idx_1 = -1;
    int cmplen;
    chapter_item_t chapter;

    while (TRUE)
    {
        if (m_bForceKill)
        {
            return FALSE;
        }

        if (!GetLine(text, m_Length - (int)(text - m_Text), &line_size, NULL, NULL, NULL, NULL))
        {
            break;
        }

        // check format
        cmplen = (int)wcslen(m_Rule->keyword);
        if (cmplen <= line_size)
        {
            bFound = FALSE;
            idx_1 = -1;
            for (int i = 0; i < line_size; i++)
            {
                if (wcsncmp(text+i, m_Rule->keyword, cmplen) == 0)
                {
                    idx_1 = i;
                    bFound = TRUE;
                    break;
                }
            }
            if (bFound)
            {
                title_len = line_size - idx_1 < (MAX_CHAPTER_LENGTH - 1) ? line_size - idx_1 : MAX_CHAPTER_LENGTH - 1;
                memcpy(title, text + idx_1, title_len * sizeof(wchar_t));
                title[title_len] = 0;

                chapter.index = /*idx_1 +*/ (int)(text - m_Text);
                chapter.title = title;
                chapter.title_len = title_len;
                m_Chapters.push_back(chapter);
            }
        }

        // set index
        text += line_size + 1; // add 0x0a
    }
    return TRUE;
}

BOOL TextBook::ParserChaptersRegex(void)
{
    wchar_t title[MAX_CHAPTER_LENGTH] = { 0 };
    int title_len = 0;
    chapter_item_t chapter;
    int offset = 0;
    std::wcmatch cm;
    std::wregex *e = NULL;
    TCHAR *text = m_Text;

    try
    {
        e = new std::wregex(m_Rule->regex);    
    }
    catch (...)
    {
        if (e)
        {
            delete e;
        }
        return FALSE;
    }

    while (std::regex_search(text, cm, *e, std::regex_constants::format_first_only))
    {
        if (m_bForceKill)
        {
            break;
        }

        title_len = (int)cm.length() < (MAX_CHAPTER_LENGTH - 1) ? (int)cm.length() : MAX_CHAPTER_LENGTH - 1;
        memcpy(title, cm.str().c_str(), title_len * sizeof(wchar_t));
        title[title_len] = 0;

        chapter.index = offset + (int)cm.position();
        chapter.title = title;
        chapter.title_len = title_len;
        m_Chapters.push_back(chapter);


        text += cm.position() + cm.length();
        offset += (int)cm.position() + (int)cm.length();
    }
    if (e)
    {
        delete e;
    }

    return TRUE;
}

BOOL TextBook::IsChapter(wchar_t* text, int len)
{
    BOOL bFound = FALSE;
    BOOL bHasNumber = FALSE;
    if (!text || len <= 0)
        return FALSE;

    for (int i = 0; i < len; i++)
    {
        bFound = FALSE;
        for (int j = 0; j < sizeof(m_ValidChapter)/sizeof(m_ValidChapter[0]); j++)
        {
            if (text[i] == m_ValidChapter[j])
            {
                bFound = TRUE;
                break;
            }
        }
        if (!bFound)
        {
            return FALSE;
        }
        if (text[i] != _T(' ') && text[i] != _T('\t') && text[i] != 0x3000)
        {
            bHasNumber = TRUE;
        }
    }
    return bHasNumber;
}
