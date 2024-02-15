/*
 *  Copyright 2011 ZXing authors All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <zxing/multi/ByQuadrantReader.h>
#include <zxing/ReaderException.h>

namespace zxing {
    namespace multi {

        ByQuadrantReader::ByQuadrantReader(Reader &delegate) : delegate_(delegate) {}

        ByQuadrantReader::~ByQuadrantReader() {}

        Ref<Result> ByQuadrantReader::decode(Ref<BinaryBitmap> image) {
            return decode(image, DecodeHints::DEFAULT_HINT);
        }

        Ref<Result> ByQuadrantReader::decode(Ref<BinaryBitmap> image, DecodeHints hints) {
            if (image.empty()) {
                return Ref<Result>();
            }
            int width = image->getWidth();
            int height = image->getHeight();
            int halfWidth = width / 2;
            int halfHeight = height / 2;

            Ref<Result> r;

            Ref<BinaryBitmap> topLeft = image->crop(0, 0, halfWidth, halfHeight);
            if(!topLeft.empty()) {
                r = delegate_.decode(topLeft, hints);
                if (!r.empty()) {
                    return r;
                }
            }

            Ref<BinaryBitmap> topRight = image->crop(halfWidth, 0, halfWidth, halfHeight);
            if(!topRight.empty()) {
                r = delegate_.decode(topRight, hints);
                if (!r.empty()) {
                    return r;
                }
            }

            Ref<BinaryBitmap> bottomLeft = image->crop(0, halfHeight, halfWidth, halfHeight);
            if(!bottomLeft.empty()) {
                r = delegate_.decode(bottomLeft, hints);
                if (!r.empty()) {
                    return r;
                }
            }

            Ref<BinaryBitmap> bottomRight = image->crop(halfWidth, halfHeight, halfWidth, halfHeight);

            if(!bottomRight.empty()) {
                r = delegate_.decode(bottomRight, hints);
                if (!r.empty()) {
                    return r;
                }
            }

            int quarterWidth = halfWidth / 2;
            int quarterHeight = halfHeight / 2;
            Ref<BinaryBitmap> center = image->crop(quarterWidth, quarterHeight, halfWidth, halfHeight);

            if(!center.empty()) {
                r = delegate_.decode(center, hints);
                if (!r.empty()) {
                    return r;
                }
            }
            return Ref<Result>();
        }

    } // End zxing::multi namespace
} // End zxing namespace
