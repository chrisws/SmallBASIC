package net.sourceforge.smallbasic;

import android.bluetooth.BluetoothSocket;
import android.util.Log;

import java.io.IOException;
import java.io.InputStream;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Thread for receiving data on a bluetooth connection
 */
public class BluetoothRxThread extends Thread {
  private static final String TAG = "smallbasic";
  private static final int RECEIVE_BUFFER_SIZE = 1024;
  private static final int RING_BUFFER_SIZE = 4096;
  private final AtomicBoolean _running;
  private final InputStream _inputStream;
  private final RingBuffer _ringBuffer;

  public BluetoothRxThread(BluetoothSocket socket) throws IOException {
    this._inputStream = socket.getInputStream();
    this._ringBuffer = new RingBuffer(RING_BUFFER_SIZE);
    this._running = new AtomicBoolean(true);
    start();
  }

  public boolean isRunning() {
    return _running.get();
  }

  public String read() {
    return _ringBuffer.read();
  }

  @Override
  public void run() {
    try {
      while (_running.get() && !Thread.currentThread().isInterrupted()) {
        byte[] buffer = new byte[RECEIVE_BUFFER_SIZE];
        int bytesRead = _inputStream.read(buffer);
        if (bytesRead > 0) {
          _ringBuffer.write(buffer, bytesRead);
        }
      }
    } catch (Exception e) {
      Log.d(TAG, "Run failed:", e);
    } finally {
      _running.set(false);
    }
    Log.d(TAG, "Bluetooth RX thread terminated");
  }

  public void stopThread() {
    _running.set(false);
    interrupt();
  }
}
