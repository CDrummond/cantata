/*
    Copyright 2006 Kevin Ottens <ervin@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PREDICATEPARSE_H
#define PREDICATEPARSE_H

void PredicateLexer_unknownToken(const char* text);

void PredicateParse_setResult(void *result);
void PredicateParse_errorDetected(const char* error);
void PredicateParse_destroy(void *pred);

void *PredicateParse_newAtom(char *interface, char *property, void *value);
void *PredicateParse_newMaskAtom(char *interface, char *property, void *value);
void *PredicateParse_newIsAtom(char *interface);
void *PredicateParse_newAnd(void *pred1, void *pred2);
void *PredicateParse_newOr(void *pred1, void *pred2);
void *PredicateParse_newStringValue(char *val);
void *PredicateParse_newBoolValue(int val);
void *PredicateParse_newNumValue(int val);
void *PredicateParse_newDoubleValue(double val);
void *PredicateParse_newEmptyStringListValue();
void *PredicateParse_newStringListValue(char *name);
void *PredicateParse_appendStringListValue(char *name, void *list);

#endif
