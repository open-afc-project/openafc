import * as React from "react";
import {
  PageSection, Title
} from "@patternfly/react-core";
import { PAWSInterface } from "./PAWSInterface";

// VirtualAP.tsx: main component for virtual ap page
// author: Sam Smucny

const VirtualAP: React.FunctionComponent = () => {
  return (
    <PageSection>
      <Title size="lg">Virtual AP</Title>
      <br/>
      <PAWSInterface/>
    </PageSection>
  );
}

export { VirtualAP };
