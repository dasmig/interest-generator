"""Map a Factbook religion label to one of the major groups."""
import re

GROUPS = {
    "unaffiliated": r"\bnone\b|unaffiliated|atheist|no religion|agnostic|non-?religio|no affiliation|nonbeliever|nothing in particular",
    "christian": r"christ|catholic|protestant|orthodox|anglican|lutheran|pentecost|evangel|baptist|coptic|adventist|methodist|presbyter|reformed|mormon|jehovah|apostolic|church|kimbangu",
    "muslim": r"muslim|sunni|shia|shi'a|islam|ibadi|alawite",
    "hindu": r"hindu",
    "buddhist": r"buddh",
    "jewish": r"jew|judais",
}


def group_of(label):
    for g, pat in GROUPS.items():
        if re.search(pat, label.lower()):
            return g
    return "other"
