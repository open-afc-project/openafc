import React from 'react';
import { createRoot } from 'react-dom/client';
import { App } from './app/index';
import { getGuiConfig } from './app/Lib/RatApi';
import '@patternfly/react-core/dist/styles/base.css';

/**
 * index.tsx: root react file that is entry point
 * author: Sam Smucny
 */

const conf = getGuiConfig();

const container = document.getElementById('root');
if (container) {
  const root = createRoot(container);
  root.render(<App conf={conf} />);
}
