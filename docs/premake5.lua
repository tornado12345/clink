-- Copyright (c) 2012 Martin Ridgers
-- License: http://opensource.org/licenses/MIT

--------------------------------------------------------------------------------
local function generate_file(source_path, out)
    print("  << " .. source_path)
    for line in io.open(source_path, "r"):lines() do
        local include = line:match("%$%(INCLUDE +([^)]+)%)")
        if include then
            generate_file(include, out)
        else
            line = line:gsub("%$%(CLINK_VERSION%)", clink_git_name:upper())
            out:write(line .. "\n")
        end
    end
end

--------------------------------------------------------------------------------
local function parse_doc_tags_impl(out, file)
    print("Parse tags: "..file)

    local line_reader = io.lines(file)
    local prefix = "///"

    -- Reads a tagged line, extracting its key and value; '/// key: value'
    local function read_tagged()
        local line = line_reader()
        if not line then
            return line
        end

        local left, right = line:find("^"..prefix.."%s+-*")
        if not left then return nil end

        line = line:sub(right + 1)
        local _, _, tag, value = line:find("^([a-z]+):%s+(.+)")
        if not tag then
            _, _, value = line:find("^%s*(.+)")
            if value then
                tag = "desc"
            end
        end

        return tag, value
    end

    -- Finds '/// name: ...' tagged lines. Denotes opening of a odocument block.
    local function parse_tagged(line)
        prefix = "///"
        local left, right = line:find("^///%s+-name:%s+")
        if not left then
            prefix = "---"
            left, right = line:find("^---%s+-name:%s+")
        end

        if not left then
            return
        end

        line = line:sub(right + 1)
        local _, _, name, group = line:find("^((.+)[.:].+)$")
        if not group then
            group = "[other]"
            name = line
        end

        return group, name
    end

    for line in line_reader do
        local desc = {}

        local group, name = parse_tagged(line)
        if name then
            for tag, value in read_tagged do
                local desc_tag = desc[tag] or {}
                table.insert(desc_tag, value)
                desc[tag] = desc_tag
            end

            desc.name = { name }

            out[group] = out[group] or {}
            table.insert(out[group], desc)
        end
    end
end

--------------------------------------------------------------------------------
local function parse_doc_tags(out, glob)
    local files = os.matchfiles(glob)
    for _, file in ipairs(files) do
        parse_doc_tags_impl(out, file)
    end
end

--------------------------------------------------------------------------------
local function do_docs()
    out_path = ".build/docs/clink.html"

    os.execute("1>nul 2>nul md .build\\docs")

    -- Collect document tags from source and output them as HTML.
    local doc_tags = {}
    parse_doc_tags(doc_tags, "clink/lua/**.lua")
    parse_doc_tags(doc_tags, "clink/lua/**.cpp")

    local groups = {}
    for group_name, group_table in pairs(doc_tags) do
        group_table.name = group_name
        table.insert(groups, group_table)
    end
    table.sort(groups, function (a, b) return a.name < b.name end)

    local api_html = io.open(".build/docs/api_html", "w")
    for _, group in ipairs(groups) do
        table.sort(group, function (a, b) return a.name[1] < b.name[1] end)

        api_html:write('<div class="group">')
        api_html:write('<h5 class="group_name">'..group.name..'</h5>')

        for _, doc_tag in ipairs(group) do
            api_html:write('<div class="function">')

            local name = doc_tag.name[1]
            local arg = table.concat(doc_tag.arg or {}, ", ")
            local ret = (doc_tag.ret or { "nil" })[1]
            local desc = table.concat(doc_tag.desc or {}, " ")
            local show = table.concat(doc_tag.show or {}, "<br/>")

            api_html:write('<div class="header">')
                api_html:write(' <div class="name">'..name..'</div>')
                api_html:write(' <div class="signature">('..arg..') : '..ret..'</div>')
            api_html:write('</div>')

            api_html:write('<div class="body">')
                api_html:write('<p class="desc">'..desc..'</p>')
                if #show > 0 then
                    api_html:write('<p class="show">'..show..'</p>')
                end
            api_html:write("</div>")

            api_html:write("</div>")
            api_html:write("<hr/>")
        end

        api_html:write("</div>")
    end
    api_html:close()

    -- Expand out template.
    print("")
    print(">> " .. out_path)
    generate_file("docs/clink.html", io.open(out_path, "w"))
    print("")
end



--------------------------------------------------------------------------------
newaction {
    trigger = "docs",
    description = "Generates Clink's documentation.",
    execute = do_docs,
}
