
/*
 * Copyright 2012 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkPaintOptionsAndroid.h"
#include "SkFlattenableBuffers.h"
#include "SkTDict.h"
#include "SkThread.h"
#include <cstring>

#ifdef SK_BUILD_FOR_ANDROID

SkLanguage SkLanguage::getParent() const {
    SkASSERT(!fTag.isEmpty());
    const char* tag = fTag.c_str();

    // strip off the rightmost "-.*"
    char* parentTagEnd = strrchr(tag, '-');
    if (parentTagEnd == NULL) {
        return SkLanguage();
    }
    size_t parentTagLen = parentTagEnd - tag;
    return SkLanguage(tag, parentTagLen);
}

void SkPaintOptionsAndroid::flatten(SkFlattenableWriteBuffer& buffer) const {
    buffer.writeUInt(fFontVariant);
    buffer.writeString(fLanguage.getTag().c_str());
    buffer.writeBool(fUseFontFallbacks);
}

void SkPaintOptionsAndroid::unflatten(SkFlattenableReadBuffer& buffer) {
    fFontVariant = (FontVariant)buffer.readUInt();
    SkString tag;
    buffer.readString(&tag);
    fLanguage = SkLanguage(tag);
    fUseFontFallbacks = buffer.readBool();
}

#endif
