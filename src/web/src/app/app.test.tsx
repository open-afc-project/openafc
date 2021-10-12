import React from "react";
import { App } from "@app/index";
import { mount, shallow } from "enzyme";
import { Button } from "@patternfly/react-core";
import { GuiConfig } from "./Lib/RatApiTypes";
import { getDefaultAfcConf, guiConfig } from "./Lib/RatApi";

Object.assign(guiConfig, {
  paws_url: "/dummy/paws",
  afcconfig_defaults: "/dummy/afc-config",
  google_apikey: "invalid-key"
})

describe("App tests", () => {
  test("should render default App component", () => {
    const view = shallow(<App conf={Promise.resolve()} />);
  });

  it("should render a nav-toggle button", () => {
    const wrapper = mount(<App conf={Promise.resolve()} />);
    const button = wrapper.find(Button);
    expect(button.exists()).toBe(true)
  });

  it("should hide the sidebar when clicking the nav-toggle button", () => {
    const wrapper = mount(<App conf={Promise.resolve()} />);
    const button = wrapper.find("#nav-toggle").hostNodes();
    expect(wrapper.find("#page-sidebar").hasClass("pf-m-expanded"));
    button.simulate("click");
    expect(wrapper.find("#page-sidebar").hasClass("pf-m-collapsed"));
  });
});
