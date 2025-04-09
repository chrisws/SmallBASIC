package net.sourceforge.smallbasic;

public class ThreadUtil {
  private ThreadUtil() {
    // no access
  }

  /**
   * Sleep or short period to avoid busy-waiting
   */
  public static void sleep() {
    try {
      Thread.sleep(10);
    } catch (InterruptedException e) {
      // ignored
    }
  }
}
