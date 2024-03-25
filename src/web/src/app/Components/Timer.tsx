import * as React from 'react';
import { logger } from '../Lib/Logger';

/**
 * Timer.tsx: simple timer element that counts the seconds since being rendered and stops when unmounted
 * author: Sam Smucny
 */

/**
 * Timer component that counts how many seconds it has been alive.
 */
export class Timer extends React.Component<{}, { count: number; timer: any }> {
  constructor(props: Readonly<{}>) {
    super(props);
    this.state = {
      count: 0,
      timer: setInterval(() => this.setState((st, pr) => ({ count: st.count + 1 })), 1000),
    };
  }

  componentWillUnmount() {
    logger.info('Timer stopped at ' + this.state.count + ' seconds. Timer: ', this.state.timer);
    clearInterval(this.state.timer);
  }

  render = () => <div>Seconds elapsed: {this.state.count}</div>;
}
