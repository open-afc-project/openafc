import React from 'react';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { App } from '@app/index';
import { guiConfig } from './Lib/RatApi';

Object.assign(guiConfig, {
  paws_url: '/dummy/paws',
  afcconfig_defaults: '/dummy/afc-config',
  google_apikey: 'invalid-key',
});

describe('App tests', () => {
  test('should render default App component', () => {
    const { container } = render(<App conf={Promise.resolve()} />);
    expect(container).toBeTruthy();
  });
});
