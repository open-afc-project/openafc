import React from 'react';
import { Card, CardHeader, CardTitle, CardBody, ClipboardCopy } from '@patternfly/react-core';

/**
 * JsonRawDisp.tsx: Simple raw JSON display that can be used for debugging
 * author: Sam Smucny
 */

/**
 * Displays JSON object in an expandable text area that can be coppied from.
 * @param props JSON value to display
 */
export const JsonRawDisp: React.FunctionComponent<{ value?: any }> = (props) =>
  props.value === undefined || props.value === null ? (
    <Card />
  ) : (
    <Card>
      <CardHeader>
        <CardTitle>Raw JSON</CardTitle>
      </CardHeader>
      <CardBody>
        <ClipboardCopy isReadOnly variant="expansion" isCode={true}>
          {!!props.value && typeof props.value === 'string' ? props.value : JSON.stringify(props.value, undefined, 2)}
        </ClipboardCopy>
      </CardBody>
    </Card>
  );
