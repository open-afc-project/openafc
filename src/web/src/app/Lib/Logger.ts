/**
 * Logger.ts: logging module for web app
 * author: Sam Smucny
 */

/**
 * Global logger with configurable log levels in source code.
 * At build time the if statement is compiled out leaving only one of the two branches.
 * Usage is identical to console.{log|warn|error}().
 *
 * ```
 * logger.info('info logging...', ...);
 * logger.warn('warn logging...', ...);
 * logger.error('error logging...', ...);
 * ```
 */
const logger = {
  /**
   * Log informative messages to console
   * ```
   * logger.info('helpful information...');
   * ```
   */
  info: false ? console.log : () => {},

  /**
   * Log warning messages to console
   * ```
   * logger.warn('This is a warning!');
   * ```
   */
  warn: false ? console.warn : () => {},

  /**
   * Low error messages to console
   * ```
   * logger.error('ERROR!');
   * ```
   */
  error: true ? console.error : () => {},
};

export { logger };
