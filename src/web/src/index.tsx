import React from "react";
import ReactDOM from "react-dom";
import { App } from "./app/index";
import { getGuiConfig } from "./app/Lib/RatApi";
import "@patternfly/react-core/dist/styles/base.css";

/**
 * index.tsx: root react file that is entry point
 * author: Sam Smucny
 */


if (process.env.NODE_ENV !== "production") {
  // tslint:disable-next-line
  const axe = require("react-axe");
  axe(React, ReactDOM, 1000);
}

// Load gui config from server. This returns a promise which is resolved in the App.
const conf = getGuiConfig();

ReactDOM.render(<App conf={conf} />, document.getElementById("root") as HTMLElement);
