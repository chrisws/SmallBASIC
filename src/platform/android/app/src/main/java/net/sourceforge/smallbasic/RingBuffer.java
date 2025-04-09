package net.sourceforge.smallbasic;

import java.nio.charset.StandardCharsets;

/**
 * RingBuffer for receiving data
 */
public class RingBuffer {
  private static final int MAX_READ_LENGTH = 80;
  private final byte[] buffer;
  private int head;
  private int tail;
  private int size;

  RingBuffer(int capacity) {
    buffer = new byte[capacity];
    head = 0;
    tail = 0;
    size = 0;
  }

  synchronized boolean hasData() {
    return size > 0;
  }

  synchronized String read() {
    int bytesToRead = Math.min(MAX_READ_LENGTH, size);
    byte[] output = new byte[bytesToRead];

    for (int i = 0; i < bytesToRead; i++) {
      output[i] = buffer[head];
      head = (head + 1) % buffer.length;
      size--;
    }

    return new String(output, StandardCharsets.UTF_8);
  }

  synchronized void write(byte[] data, int length) {
    if (length > buffer.length - size) {
      // Buffer overflow, cannot write
      return;
    }

    for (int i = 0; i < length; i++) {
      buffer[tail] = data[i];
      tail = (tail + 1) % buffer.length;
      size++;
    }
  }
}
