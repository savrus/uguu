# -*- coding: utf-8 -*-
# webuguu.faq.views - faq view for django framework
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

from django.shortcuts import render_to_response
import string
from webuguu.common import known_filetypes, known_protocols

def bold(x):
    return u"<b>%s</b>" % x

def strargs(args):
    return string.join([bold(x) for x in args], ", ")

ru_sizedesc = u" Размером является десятичное число с последующим спецификатором. Спецификатором может быть одно из: tb, gb, mb, kb, byte."

ru_multiarg = u" Moжно задать несколько аргументов через запятую, но не разделяя пробелом."

ru_orders = {
    'avl':      u"доступность шары",
    'scan':     u"время последнего сканирования",
    'uptime':   u"время последнего изменения доступности",
    'proto':    u"протокол",
    'net':      u"сеть",
    'host':     u"имя хоста",
    'size':     u"размер файла",
    'name':     u"имя файла",
    'type':     u"тип файла",
    'sharesize':     u"размер шары целиком",
}

ru_matches = {
    'name':     u"полнотекстовый поиск в имени файла",
    'full':     u"полнотекстовый поиск в полном пути",
    'exact':    u"точное совпадение имени файла (будьте внимательны, между именем файла в запросе и опциями должен быть ровно один пробел)",
}

ru_outs = {
    'html':     u"обычный вывод в HTML",
    'rss':      u"вывод в формате RSS-ленты (возможно, вам будет полезно также указать <b>order:name.d</b> при запросе)",
}

def ru_arg_desc(option):
    return string.join([ u"%s: %s" % (bold(x), option[x]) for x in option.keys()], ", ")


ru_qoptions = (
    {'option': "match",
     'args': strargs(ru_matches.keys()),
     'desc': u"Критерий поиска. Аргументы означают следующее. " + ru_arg_desc(ru_matches) + u"."
    },
    {'option': "max",
     'args': strargs(["100gb", "256.8mb"]),
     'desc': u"Максимальный размер файла." + ru_sizedesc,
    },
    {'option': "min",
     'args': strargs(["100gb", "256.8mb"]),
     'desc': u"Минимальный размер файла." + ru_sizedesc
    },
    {'option': "type",
     'args': strargs(known_filetypes),
     'desc': u"Искать только среди файлов данного типа." + ru_multiarg
    },
    {'option': "proto",
     'args': strargs(known_protocols),
     'desc': u"Искать только в шарах с заданным протоколом." + ru_multiarg,
    },
    {'option': "host",
     'args': "<i>hostname</i>",
     'desc': u"Искать только в шарах с заданным именем хоста." + ru_multiarg,
    },
    {'option': "port",
     'args': "<i>integer</i>",
     'desc': u"Искать только в шарах на заданном порту. Для шар на дефолтном порту нужно указывать значение 0." + ru_multiarg,
    },
    {'option': "network",
     'args': "<i>network</i>",
     'desc': u"Искать только в заданной сети." + ru_multiarg,
    },
    {'option': "avl",
     'args': strargs(["online", "offline"]),
     'desc': u"Искать только в доступных/недоступных шарах",
    },
    {'option': "order",
     'args': strargs(tuple(ru_orders.keys()) + \
                     tuple(["%s.d" % x for x in ru_orders.keys()])),
     'desc': u"Сортировать результаты в заданном порядке. Aргументы с суффиксом <b>.d</b> означают сортировку в обратном порядке." + ru_multiarg + u" Аргументы означают следующее. " + ru_arg_desc(ru_orders) + u".",
    },
    {'option': "out",
     'args': strargs(["html", "rss"]),
     'desc': u"Формат вывода результатов поиска. " + ru_arg_desc(ru_outs) + u".",
    },
)


def ru(request):
    return render_to_response("faq/ru.html",
        {'options': ru_qoptions,
         'gpage': "http://code.google.com/p/uguu",
         'ggroup': "http://groups.google.com/group/uguu",
        })

