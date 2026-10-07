package org.vetalguru.balcalc

/**
 * A picture made small for the database: JPEG, the longer side at most
 * [maxSide] pixels; null when [image] is not a picture.
 */
expect fun shrinkImage(image: ByteArray, maxSide: Int = 640): ByteArray?

/** A picture edit to save with a rifle or cartridge: new bytes, or null to remove it. */
class PhotoChange(val image: ByteArray?)
