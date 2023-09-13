import React, { useState } from "react";
import { AboutModal, Button, TextContent, TextList, TextListItem } from "@patternfly/react-core";
import { Accordion, AccordionItem, AccordionContent, AccordionToggle } from '@patternfly/react-core';
import { guiConfig } from "../Lib/RatApi";
import { InfoIcon } from "@patternfly/react-icons";

/**
 * AppInfo.tsx: Application about modal with version information
 * author: Sam Smucny
 */

/**
 * Modal component that provides information about the application
 */
class AppInfo extends React.Component<{}, { isModalOpen: boolean, expanded: string[] }> {
    setExpanded(newExpanded: string[]) {
        this.setState(Object.assign({ expanded: newExpanded }));
    }
    handleModalToggle: () => void;

    constructor(props: any) {
        super(props);
        this.state = {
            isModalOpen: false,
            expanded: []
        };
        this.handleModalToggle = () => {
            this.setState(({ isModalOpen }) => ({
                isModalOpen: !isModalOpen
            }));
        };
    }


    toggle = id => {
        const index = this.state.expanded.indexOf(id);
        const newExpanded: string[] =
            index >= 0 ? [...this.state.expanded.slice(0, index), ...this.state.expanded.slice(index + 1, this.state.expanded.length)] : [...this.state.expanded, id];
        this.setExpanded(newExpanded);
    };


    render() {
        const { isModalOpen } = this.state;

        return (
            <React.Fragment>
                <Button variant="plain" isInline={true} onClick={this.handleModalToggle}>
                    <InfoIcon />
                </Button>
                <AboutModal
                    isOpen={isModalOpen}
                    onClose={this.handleModalToggle}
                    trademark="The use of this software shall be governed by the terms and conditions of the OpenAFC Project License."
                    brandImageSrc="images/logo.png"
                    brandImageAlt="Logo"
                    backgroundImageSrc='images/background.png'
                    productName={guiConfig.app_name}
                >
                    <TextContent id="parametersList">
                        <TextList component="dl">
                            <TextListItem component="dt">Version</TextListItem>
                            <TextListItem component="dd">{guiConfig.version}</TextListItem>
                        </TextList>
                    </TextContent>
                    <h2>Database Licences and Source Citations</h2>
                    <Accordion id="aboutAccordion" asDefinitionList={false} label="Database Licences and Source Citations">
                        <AccordionItem>
                            <AccordionToggle
                                onClick={() => this.toggle('ex2-toggle1')}
                                isExpanded={this.state.expanded.includes('ex2-toggle1')}
                                id="ex2-toggle1"
                            >
                                NLCD
                            </AccordionToggle>
                            <AccordionContent id="ex2-expand1" isHidden={!this.state.expanded.includes('ex2-toggle1')} isFixed>
                                <p>
                                    Public domain
                                </p><p>
                                    References:
                                </p><p>
                                    Dewitz, J., and U.S. Geological Survey, 2021, National Land Cover Database (NLCD) 2019 Products (ver. 2.0, June 2021): U.S. Geological Survey data release, https://doi.org/10.5066/P9KZCM54
                                </p><p>
                                    Wickham, J., Stehman, S.V., Sorenson, D.G., Gass, L., and Dewitz, J.A., 2021, Thematic accuracy assessment of the NLCD 2016 land cover for the conterminous United States: Remote Sensing of Environment, v. 257, art. no. 112357, at https://doi.org/10.1016/j.rse.2021.112357
                                </p><p>
                                    Homer, Collin G., Dewitz, Jon A., Jin, Suming, Xian, George, Costello, C., Danielson, Patrick, Gass, L., Funk, M., Wickham, J., Stehman, S., Auch, Roger F., Riitters, K. H., Conterminous United States land cover change patterns 2001–2016 from the 2016 National Land Cover Database: ISPRS Journal of Photogrammetry and Remote Sensing, v. 162, p. 184–199, at https://doi.org/10.1016/j.isprsjprs.2020.02.019
                                </p><p>
                                    Jin, Suming, Homer, Collin, Yang, Limin, Danielson, Patrick, Dewitz, Jon, Li, Congcong, Zhu, Z., Xian, George, Howard, Danny, Overall methodology design for the United States National Land Cover Database 2016 products: Remote Sensing, v. 11, no. 24, at https://doi.org/10.3390/rs11242971
                                </p><p>
                                    Yang, L., Jin, S., Danielson, P., Homer, C., Gass, L., Case, A., Costello, C., Dewitz, J., Fry, J., Funk, M., Grannemann, B., Rigge, M. and G. Xian. 2018. A New Generation of the United States National Land Cover Database: Requirements, Research Priorities, Design, and Implementation Strategies, ISPRS Journal of Photogrammetry and Remote Sensing, 146, pp.108-123.
                                </p>
                            </AccordionContent>
                        </AccordionItem>
                        <AccordionItem>
                            <AccordionToggle
                                onClick={() => this.toggle('ex2-toggle2')}
                                isExpanded={this.state.expanded.includes('ex2-toggle2')}
                                id="ex2-toggle2"
                            >
                                proc_lidar_2019
                            </AccordionToggle>
                            <AccordionContent id="ex2-expand2" isHidden={!this.state.expanded.includes('ex2-toggle2')} isFixed>
                                <p>
                                    Available for public use with no restrictions
                                </p><p>
                                    Disclaimer and quality information is at https://rockyweb.usgs.gov/vdelivery/Datasets/Staged/Elevation/Non_Standard_Contributed/NGA_US_Cities/00_NGA%20133%20US%20Cities%20Data%20Disclaimer%20and%20Explanation%20Readme.pdf
                                </p>
                            </AccordionContent>
                        </AccordionItem>
                        <AccordionItem>
                            <AccordionToggle
                                onClick={() => this.toggle('ex2-toggle3')}
                                isExpanded={this.state.expanded.includes('ex2-toggle3')}
                                id="ex2-toggle3"
                            >
                                3DEP
                            </AccordionToggle>
                            <AccordionContent id="ex2-expand3" isHidden={!this.state.expanded.includes('ex2-toggle3')} isFixed>
                                <p>Public domain
                                </p><p>
                                    Data available from U.S. Geological Survey, National Geospatial Program.</p>
                            </AccordionContent>
                        </AccordionItem>
                        <AccordionItem>
                            <AccordionToggle
                                onClick={() => this.toggle('ex2-toggle4')}
                                isExpanded={this.state.expanded.includes('ex2-toggle4')}
                                id="ex2-toggle4"
                            >
                                Globe
                            </AccordionToggle>
                            <AccordionContent id="ex2-expand4" isHidden={!this.state.expanded.includes('ex2-toggle4')} isFixed>
                                <p>
                                    Public domain
                                </p><p>
                                    NOAA National Geophysical Data Center. 1999: Global Land One-kilometer Base Elevation (GLOBE) v.1. NOAA National Centers for Environmental Information. https://doi.org/10.7289/V52R3PMS. Accessed TBD
                                </p>
                            </AccordionContent>
                        </AccordionItem>
                        <AccordionItem>
                            <AccordionToggle
                                onClick={() => this.toggle('ex2-toggle5')}
                                isExpanded={this.state.expanded.includes('ex2-toggle5')}
                                id="ex2-toggle5"
                            >
                                population
                            </AccordionToggle>
                            <AccordionContent id="ex2-expand5" isHidden={!this.state.expanded.includes('ex2-toggle5')} isFixed>
                                <p>Creative Commons Attribution 4.0 International (CC BY) License (https://creativecommons.org/licenses/by/4.0)
                                </p><p>
                                    Center for International Earth Science Information Network - CIESIN - Columbia University. 2018. Gridded Population of the World, Version 4 (GPWv4): Population Density, Revision 11. Palisades, New York: NASA Socioeconomic Data and Applications Center (SEDAC). https://doi.org/10.7927/H49C6VHW
                                </p>
                            </AccordionContent>
                        </AccordionItem>
                    </Accordion>
                </AboutModal>
            </React.Fragment>
        );
    }
}

export { AppInfo };
